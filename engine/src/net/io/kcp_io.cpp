#include "kcp_io.hpp"

#if defined(ENABLE_KCP)

#include "ev/ev_watcher.hpp"
#include "net/io/net_io_helper.hpp"
#include "thread/thread_local_buf.hpp"

#ifdef __windows__
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
#endif

KcpIO::KcpIO()
{
}

KcpIO::~KcpIO()
{
    // 兜底：正常路径下 KCP_DEL 已经在 KcpMgr::remove() 里 release 过，
    // 这里只是防止漏网。
    // ★ kcp_ 的生死由 KcpMgr 独占，所以 ~KcpIO 无论跑在哪个线程都不会碰
    //   正在被 backend 使用的 ikcpcb
    release_kcp();
}

/**
 * 唯一真正 sendto 的地方，跑在 backend 线程
 * 对端地址直接用 peer_，不需要每帧携带（见设计 §2.2）
 */
int32_t KcpIO::output(const char *buf, int32_t len, struct IKCPCB * /*kcp*/,
                      void *user)
{
    KcpIO *io = static_cast<KcpIO *>(user);
    int32_t fd = io->listen_fd_;
    if (fd == netcompat::INVALID) return -1;

    // 客户端形态：socket已connect，对端地址是默认值，直接send
    if (io->peer_.is_default()) return (int32_t)::send(fd, buf, len, 0);

    struct sockaddr_storage ss;
    socklen_t sl = io->peer_.to_sockaddr(ss);
    if (0 == sl) return -1;

    return (int32_t)::sendto(fd, buf, len, 0, (struct sockaddr *)&ss, sl);
}

bool KcpIO::create_kcp(uint32_t conv, int32_t listen_id, int32_t listen_fd,
                       const UdpAddr &addr)
{
    assert(!kcp_);

    conv_      = conv;
    listen_id_ = listen_id;
    listen_fd_ = listen_fd;
    peer_      = addr;

    kcp_ = ikcp_create(conv, this);
    if (!kcp_) return false;

    ikcp_setoutput(kcp_, &KcpIO::output);
    ikcp_setmtu(kcp_, KCP_MTU);
    ikcp_nodelay(kcp_, KCP_NODELAY, KCP_INTERVAL, KCP_RESEND, KCP_NC);
    ikcp_wndsize(kcp_, KCP_SND_WND, KCP_RCV_WND);

    return true;
}

void KcpIO::release_kcp()
{
    if (!kcp_) return; // ★ 幂等：KcpMgr::remove 与 ~KcpIO 都可能调
    ikcp_release(kcp_);
    kcp_ = nullptr;
}

int32_t KcpIO::input(const char *data, int32_t len, bool &has_data)
{
    has_data = false;
    if (!kcp_) return -1;

    int32_t ret = ikcp_input(kcp_, data, len);
    if (0 != ret) return ret;

    // ikcp_input 内部会更新rtt/ack队列，但不flush
    has_data = drain();
    return 0;
}

bool KcpIO::drain()
{
    bool produced = false;
    // 变长缓冲：ikcp 用 rcv_wnd * mss 限制单条逻辑包的大小（约176KB，比
    // KCP_MAX_MSG大），所以这里不能开定长缓冲，get() 时按需伸缩
    thread_local ThreadLocalBuf<sizeof(uint32_t) + KCP_MAX_MSG,
                                2 * 1024 * 1024>
        buf;

    while (true)
    {
        // ★ 是 < 0 而不是 <= 0：0 表示"一条长度为0的完整逻辑包"，
        //   必须消费掉，否则它会一直卡在队列头上，后面的消息全被堵住
        int32_t peek = ikcp_peeksize(kcp_);
        if (peek < 0) break;

        if (recv_.is_overflow()) break; // 交给上层按 M_OVERFLOW_* 处理

        int32_t need = (int32_t)sizeof(uint32_t) + peek;
        char *pbuf   = buf.get((size_t)need);

        int32_t n = ikcp_recv(kcp_, pbuf + sizeof(uint32_t), peek);
        if (n < 0) break;

        if (0 == n) continue; // 空逻辑包，跳过（上面已经把它消费掉了）

        // 超过KCP_MAX_MSG的逻辑包（对端不是本框架）直接丢弃，不能塞进recv_，
        // 否则会撑爆上层按KCP_MAX_MSG来算的各种缓冲区
        if (n > KCP_MAX_MSG)
        {
            ELOG("kcp drop over max msg: %d", n);
            continue;
        }

        uint32_t size = (uint32_t)(sizeof(uint32_t) + n);
        memcpy(pbuf, &size, sizeof(size));

        recv_.append(pbuf, size); // 一次append整帧
        produced = true;
    }
    return produced;
}

int32_t KcpIO::send(EVIO *w)
{
    if (!kcp_) return EV_NONE;

    while (true)
    {
        uint32_t size = 0;
        if (send_.length() < (int64_t)sizeof(size)) break;
        if (!send_.peek(&size)) break;

        // 帧不完整，等下次。★ 返回EV_NONE而不是EV_WRITE：
        // kcp的可写性由ikcp_waitsnd和定时器决定，不靠epoll可写
        if (send_.length() < (int64_t)size) break;

        if (size < sizeof(uint32_t)
            || size > (uint32_t)(sizeof(uint32_t) + KCP_MAX_MSG))
        {
            assert(false);
            return EV_ERROR;
        }

        char *frame = send_.peek_buffer(size, 2);
        if (!frame) break;

        const char *payload = frame + sizeof(uint32_t);
        int32_t len         = (int32_t)(size - sizeof(uint32_t));

        // ★ 背压：丢这一条 + 计数，绝不sleep、绝不因一个对端卡住整条fd
        if (ikcp_waitsnd(kcp_) >= KCP_MAX_WAIT_SND)
        {
            ++stat_drop_snd_;
            ELOG("kcp send drop, conn=%d waitsnd=%d", w->id_, ikcp_waitsnd(kcp_));
            send_.remove_head_data(size);
            continue;
        }

        if (0 != ikcp_send(kcp_, payload, len)) ++stat_drop_snd_;
        send_.remove_head_data(size);

        // ★ ikcp_send 只是入队；ikcp_update 要等 ts_flush 才flush，
        //   所以这里必须显式flush，否则白等一个interval(40ms)
        ikcp_flush(kcp_);
    }

    return EV_NONE;
}

int32_t KcpIO::recv(EVIO *w)
{
    // 会话已被 KcpMgr 回收（ikcpcb已释放），等常规关闭路径收尾即可，
    // 不要在这里报错，否则会把一次正常关闭变成错误关闭
    if (!kcp_) return EV_NONE;

    int32_t fd = w->fd_;
    assert(fd != netcompat::INVALID);

    thread_local ThreadLocalBuf<UDP_MAX_DGRAM> buf;
    bool has_data = false;

    for (int32_t i = 0; i < MAX_RECV_PER_EVENT; i++)
    {
        // 已connect，用::recv就够，不需要recvfrom（对端地址固定）
        int32_t n = (int32_t)::recv(fd, buf.get(), UDP_MAX_DGRAM, 0);
        if (n < 0)
        {
            int32_t e = netcompat::errorno();
            if (!netcompat::iserror(e)) break;    // EAGAIN，正常结束
            if (is_icmp_unreachable(e)) continue; // 忽略ICMP不可达

            w->errno_ = e;
            ELOG("kcp recv fd=%d:%s(%d)", fd, netcompat::strerror(e), e);
            return EV_ERROR;
        }

        bool d = false;
        if (0 != input(buf.get(), n, d))
        {
            w->errno_ = EBADMSG;
            return EV_ERROR;
        }
        has_data = has_data || d;
    }

    // 纯ACK没有业务数据 → 返回EV_NONE，避免白唤醒worker
    return has_data ? EV_READ : EV_NONE;
}

#endif
