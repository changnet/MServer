#include "udp_io.hpp"

#include <cstddef> // offsetof

#include "ev/ev_watcher.hpp"
#include "net/io/net_io_helper.hpp" // is_icmp_unreachable
#include "net/net_compat.hpp"
#include "thread/thread_local_buf.hpp"

#ifdef __windows__
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
#endif

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

    // 预留UDP_FRAME_HEAD字节帧头，recvfrom直接把数据写到payload区。
    // 用ThreadLocal而不是裸的thread_local数组：后者会进.tbss，进程里每个线程
    // 创建时就白吃64KB，而这里只有一个backend线程会执行到
    thread_local ThreadLocalBuf<UDP_FRAME_HEAD + UDP_MAX_DGRAM> buf;

    char *pbuf = buf.get();
    for (int32_t i = 0; i < MAX_RECV_PER_EVENT; i++)
    {
        if (recv_.is_overflow()) return EV_BUSY;

        struct sockaddr_storage from;
        socklen_t from_len = sizeof(from);

        int32_t n = (int32_t)::recvfrom(fd, pbuf + UDP_FRAME_HEAD,
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
        memcpy(pbuf, &size, sizeof(size));
        memcpy(pbuf + sizeof(size), &addr, sizeof(addr));

        recv_.append(pbuf, size); // 一次append整帧
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

        // 直接按字段从帧里取，不 memcpy 整个 UdpAddr：
        //   1. 帧在缓冲区里可能是任意对齐，逐字段 memcpy 而不是 reinterpret_cast
        //   2. v4 只用到 addr_ 的前4字节，把16字节全拷出来是浪费
        //   3. 省掉 UdpAddr 临时对象和它那次构造里的清零
        uint16_t family = 0;
        uint16_t port   = 0;
        memcpy(&family, frame + sizeof(size) + offsetof(UdpAddr, family_),
               sizeof(family));
        memcpy(&port, frame + sizeof(size) + offsetof(UdpAddr, port_),
               sizeof(port));
        const uint8_t *peer =
            (const uint8_t *)(frame + sizeof(size) + offsetof(UdpAddr, addr_));

        const char *payload = frame + UDP_FRAME_HEAD;
        int32_t len         = (int32_t)(size - UDP_FRAME_HEAD);

        int32_t ret = 0;
        if (AF_UNSPEC == family)
        {
            ret = (int32_t)::send(fd, payload, len, 0); // 已connect的客户端
        }
        else if (AF_INET == family)
        {
            // 用 sockaddr_in 而不是 sockaddr_storage，避免每包 memset 128 字节
            struct sockaddr_in s4;
            memset(&s4, 0, sizeof(s4));
            s4.sin_family = AF_INET;
            s4.sin_port   = port;
            memcpy(&s4.sin_addr, peer, sizeof(s4.sin_addr));

            ret = (int32_t)::sendto(fd, payload, len, 0,
                                    (struct sockaddr *)&s4, sizeof(s4));
        }
        else if (AF_INET6 == family)
        {
            struct sockaddr_in6 s6;
            memset(&s6, 0, sizeof(s6));
            s6.sin6_family = AF_INET6;
            s6.sin6_port   = port;
            memcpy(&s6.sin6_addr, peer, sizeof(s6.sin6_addr));

            ret = (int32_t)::sendto(fd, payload, len, 0,
                                    (struct sockaddr *)&s6, sizeof(s6));
        }
        else
        {
            // 地址无效，丢弃这一帧，不然会一直卡在这里
            send_.remove_head_data(size);
            continue;
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
