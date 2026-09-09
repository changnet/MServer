#include "udp_io.hpp"

#include "ev/ev_watcher.hpp"
#include "net/net_compat.hpp"

#ifdef __windows__
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
#endif

/**
 * ICMP port unreachable：上一个sendto发送到了一个已关闭的对端
 * windows下是WSAECONNRESET，linux下是ECONNREFUSED。这不是错误，
 * 必须忽略，否则udp连接会被误判为出错并关闭
 */
static bool is_icmp_unreachable(int32_t e)
{
#ifdef __windows__
    return WSAECONNRESET == e || WSAENETRESET == e || WSAECONNABORTED == e;
#else
    return ECONNREFUSED == e || ECONNRESET == e;
#endif
}

UdpIO::UdpIO()
{
}

UdpIO::~UdpIO()
{
}

int32_t UdpIO::recv(EVIO *w)
{
    int32_t fd = w->fd_;
    assert(fd != netcompat::INVALID);

    // 预留UDP_FRAME_HEAD字节帧头，recvfrom直接把数据写到payload区
    static thread_local char buf[UDP_FRAME_HEAD + UDP_MAX_DGRAM];

    for (int32_t i = 0; i < MAX_RECV_PER_EVENT; i++)
    {
        if (recv_.is_overflow()) return EV_BUSY;

        struct sockaddr_storage from;
        socklen_t from_len = sizeof(from);

        int32_t n = (int32_t)::recvfrom(fd, buf + UDP_FRAME_HEAD,
                                        UDP_MAX_DGRAM, 0,
                                        (struct sockaddr *)&from, &from_len);
        if (n < 0)
        {
            int32_t e = netcompat::errorno();
            if (!netcompat::iserror(e)) break; // EAGAIN，正常结束

            if (is_icmp_unreachable(e)) continue; // 忽略ICMP不可达

            w->errno_ = e;
            ELOG("udp recv fd = %d:%s(%d)", fd, netcompat::strerror(e), e);
            return EV_ERROR;
        }

        UdpAddr addr;
        addr.from_sockaddr((struct sockaddr *)&from); // 内部已clear

        // 地址和帧头不会被发送出去，不需要考虑字节序
        uint32_t size = (uint32_t)(UDP_FRAME_HEAD + n);
        memcpy(buf, &size, sizeof(size));
        memcpy(buf + sizeof(size), &addr, sizeof(addr));

        recv_.append(buf, size); // 一次append整帧
    }

    return EV_NONE;
}

int32_t UdpIO::send(EVIO *w)
{
    int32_t fd = w->fd_;
    assert(fd != netcompat::INVALID);

    while (true)
    {
        uint32_t size = 0;
        if (send_.length() < (int64_t)sizeof(size)) return EV_NONE;
        if (!send_.peek(&size)) return EV_NONE;

        // 帧不完整，等下次。
        // ★ 这里必须返回EV_NONE而不是EV_WRITE：udp几乎永远可写，
        //   保留写事件会让backend busy loop。worker写完必然flush重新触发
        if (send_.length() < (int64_t)size) return EV_NONE;
        if (size < (uint32_t)UDP_FRAME_HEAD)
        {
            assert(false);
            return EV_ERROR;
        }

        // 一次拿到整帧（跨chunk时peek_buffer会拷到thread_local缓冲）
        char *frame = send_.peek_buffer(size, 2);
        if (!frame) return EV_NONE;

        // ★ 必须memcpy，不能reinterpret_cast：帧在缓冲区里可能是任意对齐
        UdpAddr addr;
        memcpy(&addr, frame + sizeof(size), sizeof(addr));

        const char *payload = frame + UDP_FRAME_HEAD;
        int32_t len         = (int32_t)(size - UDP_FRAME_HEAD);

        int32_t ret = 0;
        if (addr.is_default())
        {
            ret = (int32_t)::send(fd, payload, len, 0); // 已connect的客户端
        }
        else
        {
            struct sockaddr_storage ss;
            socklen_t slen = addr.to_sockaddr(ss);
            if (0 == slen)
            {
                // 地址无效，丢弃这一帧，不然会一直卡在这里
                send_.remove_head_data(size);
                continue;
            }
            ret = (int32_t)::sendto(fd, payload, len, 0,
                                    (struct sockaddr *)&ss, slen);
        }

        if (ret >= 0)
        {
            send_.remove_head_data(size); // 成功才消费
            continue;
        }

        int32_t e = netcompat::errorno();
        if (!netcompat::iserror(e)) return EV_WRITE; // EAGAIN，保留数据等可写

        w->errno_ = e;
        ELOG("udp send fd = %d:%s(%d)", fd, netcompat::strerror(e), e);
        return EV_ERROR;
    }
}
