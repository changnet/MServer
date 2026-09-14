#include "kcp_acceptor_io.hpp"

#if defined(ENABLE_KCP)

#include "ev/ev_watcher.hpp"
#include "ev/time.hpp"
#include "net/io/kcp_io.hpp"
#include "net/io/net_io_helper.hpp"
#include "system/static_global.hpp"
#include "thread/thread_local_buf.hpp"

#include "ikcp.h"

#ifdef __windows__
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
#endif

/// accept表项多久没晋升就回收（用户要求16秒）
static constexpr int64_t KCP_ACCEPT_TIMEOUT_MS = 16 * 1000;
/// 同一原因的日志最小间隔，防止addr洪水刷爆磁盘
static constexpr int64_t KCP_LOG_INTERVAL_MS = 1000;
/// ikcp的conv在报文最前面4字节
static constexpr int32_t KCP_CONV_LEN = 4;

KcpAcceptorIO::KcpAcceptorIO()
{
}

KcpAcceptorIO::~KcpAcceptorIO()
{
    /**
     * 监听socket一定走过 Socket::start()，也就是置了 M_REF_BACKEND，
     * 所以本对象的析构只可能发生在io线程（epoll关闭路径 → ~EVIO → delete io_），
     * 可以安全地碰 KcpMgr（同为io线程独占）
     */
    if (registered_ && StaticGlobal::B)
    {
        StaticGlobal::B->kcp_mgr().unreg_acceptor(listen_id_);
    }
}

int32_t KcpAcceptorIO::recv(EVIO *w)
{
    UNUSED(w);
    return EV_NONE;
}

int32_t KcpAcceptorIO::accept(EVIO *w)
{
    int32_t fd = w->fd_;
    assert(fd != netcompat::INVALID);

    accept_notify_ = false;

    // 首次被调用时把自己登记到KcpMgr（listen_id就是监听socket的socket_id），
    // 供 KcpMgr::on_add 晋升、以及定期回收 accept 表使用
    if (!registered_)
    {
        listen_id_  = w->id_;
        registered_ = true;
        StaticGlobal::B->kcp_mgr().reg_acceptor(listen_id_, this);
    }

    thread_local ThreadLocalBuf<UDP_MAX_DGRAM> buf;

    for (int32_t i = 0; i < MAX_RECV_PER_EVENT; i++)
    {
        struct sockaddr_storage from;
        socklen_t from_len = sizeof(from);

        int32_t n = (int32_t)::recvfrom(fd, buf.get(), UDP_MAX_DGRAM, 0,
                                        (struct sockaddr *)&from, &from_len);
        if (n < 0)
        {
            int32_t e = netcompat::errorno();
            if (!netcompat::iserror(e)) break;    // EAGAIN，正常结束
            if (is_icmp_unreachable(e)) continue; // 忽略ICMP不可达

            w->errno_ = e;
            ELOG("kcp acceptor recv fd=%d:%s(%d)", fd, netcompat::strerror(e), e);
            return EV_ERROR;
        }

        UdpAddr addr;
        addr.from_sockaddr((struct sockaddr *)&from); // 内部已clear

        on_dgram(fd, addr, buf.get(), n);
    }

    /**
     * ★ 只返回 EV_NONE/EV_ERROR，绝不能返回 EV_READ：
     *   do_io_status 的 case EV_READ 会把监听socket的注册事件从 EV_ACCEPT
     *   翻成 EV_READ，下一轮就掉进 w->recv() 分支（而且 do_io_status 没有
     *   case EV_ACCEPT，返回它只会打 "unknow io status"）。
     *   udp 是 LT 模式，缓冲区还有数据时 epoll 会继续报，不需要主动求重试
     */
    return EV_NONE;
}

void KcpAcceptorIO::on_dgram(int32_t fd, const UdpAddr &addr, const char *data,
                             int32_t len)
{
    // ① 已建立 → 直接喂给那条连接的ikcp（和数据面同线程，无锁）
    auto est = established_.find(addr);
    if (est != established_.end())
    {
        EVIO *conn_w = est->second;
        KcpIO *io    = static_cast<KcpIO *>(conn_w->io_);

        // 会话正在被 KcpMgr 回收（ikcpcb 已释放）：这一包直接丢，
        // 不要在这里报错，否则会把一次正常关闭变成协议错误
        if (!io || !io->kcp()) return;

        bool has_data = false;
        io->touch(timing::steady_clock());

        if (0 != io->input(data, len, has_data))
        {
            ELOG("kcp input error, conn=%d", conn_w->id_);
            StaticGlobal::B->kcp_mgr().remove(conn_w->id_, true); // 协议错误 → 回收
            return;
        }

        // ★ 只有确实产出业务数据才唤醒，纯ACK/探测包不唤醒业务线程
        if (has_data) StaticGlobal::B->notify_watcher(conn_w, EV_READ);
        return;
    }

    // ② 未建立 → 放进accept表
    int64_t now = timing::steady_clock();

    std::scoped_lock sl(accept_mutex_);

    auto it = accepting_.find(addr);
    if (it == accepting_.end())
    {
        if (accepting_.size() >= (size_t)KCP_MAX_PARKED)
        {
            log_limited(now, "table full", addr, KCP_MAX_PARKED);
            return;
        }

        /**
         * ikcp_getconv 要读前4字节，所以短包必须挡掉。
         * 这是内存安全，不是洪水防护 —— 洪水防护靠 KCP_MAX_PARKED + 16s回收
         */
        if (len < KCP_CONV_LEN)
        {
            log_limited(now, "short dgram", addr, len);
            return;
        }

        AcceptEntry &e = accepting_[addr];
        e.conv         = ikcp_getconv(data);
        e.listen_fd    = fd;
        e.create_ms    = now;
        e.data.assign(data, (size_t)len);

        ready_.push_back(addr);
        ++accepting_num_;
        accept_notify_ = true; // 有新对端，需要唤醒业务线程
        return;
    }

    /**
     * ③ 已经在accept表里：还没晋升（或已通知业务线程但KCP_ADD还没到），
     *    继续往这条对端的缓冲区里攒，KCP_ADD时会按序回放
     */
    AcceptEntry &e = it->second;
    if (e.data.size() + (size_t)len > (size_t)KCP_MAX_PARKED_DATA)
    {
        log_limited(now, "parked overflow", addr,
                    (int32_t)e.data.size() + len);
        return;
    }
    e.data.append(data, (size_t)len);
}

bool KcpAcceptorIO::pop_accept(UdpAddr &addr, uint32_t &conv)
{
    std::scoped_lock sl(accept_mutex_);

    while (!ready_.empty())
    {
        addr = ready_.front();
        ready_.pop_front();

        auto it = accepting_.find(addr);
        if (it == accepting_.end()) continue; // 已被拒绝/超时回收

        conv = it->second.conv;
        return true;
    }

    return false;
}

void KcpAcceptorIO::promote(const UdpAddr &addr, EVIO *w, std::string &out)
{
    {
        std::scoped_lock sl(accept_mutex_);

        auto it = accepting_.find(addr);
        if (it != accepting_.end())
        {
            out.swap(it->second.data);
            accepting_.erase(it);
            --accepting_num_;

            for (auto r = ready_.begin(); r != ready_.end(); ++r)
            {
                if (*r == addr)
                {
                    ready_.erase(r);
                    break;
                }
            }
        }
    }

    // 已建立表只在io线程被访问（on_dgram/promote/unestablish 都是io线程），
    // 所以放在锁外也安全；但不能挪到临界区之前（见头文件说明的手序）
    established_[addr] = w;
}

void KcpAcceptorIO::unestablish(const UdpAddr &addr)
{
    established_.erase(addr);
}

void KcpAcceptorIO::drop_accepting(const UdpAddr &addr)
{
    std::scoped_lock sl(accept_mutex_);

    if (0 == accepting_.erase(addr)) return;
    --accepting_num_;

    for (auto r = ready_.begin(); r != ready_.end(); ++r)
    {
        if (*r == addr)
        {
            ready_.erase(r);
            break;
        }
    }
}

bool KcpAcceptorIO::sweep(int64_t now)
{
    // 常态（没有待接入对端）下不加锁、不遍历
    if (0 == accepting_num_.load(std::memory_order_relaxed)) return false;

    std::scoped_lock sl(accept_mutex_);

    for (auto it = accepting_.begin(); it != accepting_.end();)
    {
        if (now - it->second.create_ms < KCP_ACCEPT_TIMEOUT_MS)
        {
            ++it;
            continue;
        }

        char ip[INET6_ADDRSTRLEN];
        ELOG("kcp accept timeout(%llds), drop peer=%s:%u",
             (long long)(KCP_ACCEPT_TIMEOUT_MS / 1000),
             it->first.to_string(ip, sizeof(ip)),
             (uint32_t)ntohs(it->first.port_));

        for (auto r = ready_.begin(); r != ready_.end(); ++r)
        {
            if (*r == it->first)
            {
                ready_.erase(r);
                break;
            }
        }

        it = accepting_.erase(it);
        --accepting_num_;
    }

    return !accepting_.empty();
}

void KcpAcceptorIO::log_limited(int64_t now, const char *what,
                                const UdpAddr &addr, int32_t extra)
{
    if (now - last_log_ms_ < KCP_LOG_INTERVAL_MS) return;
    last_log_ms_ = now;

    char ip[INET6_ADDRSTRLEN];
    ELOG("kcp accept %s, peer=%s:%u, extra=%d", what,
         addr.to_string(ip, sizeof(ip)), (uint32_t)ntohs(addr.port_), extra);
}

#endif
