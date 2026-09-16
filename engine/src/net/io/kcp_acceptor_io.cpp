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

KcpAcceptorIO::KcpAcceptorIO()
{
}

KcpAcceptorIO::~KcpAcceptorIO()
{
}

int32_t KcpAcceptorIO::recv(EVIO *w)
{
    UNUSED(w);
    return EV_NONE;
}

void KcpAcceptorIO::on_backend_add(EVIO *w)
{
    StaticGlobal::B->kcp_mgr().add_acceptor(w->id_, this);
}

void KcpAcceptorIO::on_backend_remove(EVIO *w)
{
    StaticGlobal::B->kcp_mgr().remove_acceptor(w->id_, this);
}

int32_t KcpAcceptorIO::accept(EVIO *w)
{
    int32_t fd = w->fd_;
    assert(fd != netcompat::INVALID);

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

        on_dgram(addr, buf.get(), n);
    }

    return EV_NONE;
}

void KcpAcceptorIO::on_dgram(const UdpAddr &addr, const char *data, int32_t len)
{
    // ① 已建立 → 直接喂给那条连接的ikcp（和数据面同线程，无锁）
    auto est = established_.find(addr);
    if (est != established_.end())
    {
        EVIO *w = est->second;
        KcpIO *io    = static_cast<KcpIO *>(w->io_);

        int32_t ret = io->input(data, len);
        if (ret > 0)
        {
            StaticGlobal::B->dispatch_event(w, EV_READ);
        }
        else if (ret < 0)
        {
            ELOG("kcp input error, conn=%d", w->id_);
            StaticGlobal::B->kcp_mgr().remove(w->id_, true); // 协议错误 → 回收
        }
        return;
    }

    // ② 未建立 → 放进accept表
    int64_t now = timing::steady_clock();

    std::scoped_lock sl(accept_mutex_);

    auto it = accepting_.find(addr);
    if (it != accepting_.end())
    {
        /**
         * 已经在accept表里：还没在业务逻辑那边建立连接，
         * 继续往这条对端的缓冲区里攒，KCP_ADD时会按序回放
         */
        AcceptEntry &e = it->second;
        if (e.data.size() + (size_t)len > (size_t)KCP_MAX_PARKED_DATA)
        {
            log_limited(now, "parked overflow", addr,
                        (int32_t)e.data.size() + len);
        }
        else
        {
            e.data.append(data, (size_t)len);
        }
        return;
    }

    if (accepting_.size() >= (size_t)KCP_MAX_PARKED)
    {
        log_limited(now, "table full", addr, KCP_MAX_PARKED);
        return;
    }

    // ikcp_getconv 不检测指针长度，要防止收到其他程序的非kcp包
    IUINT32 conv = 0;
    if (len < sizeof(conv))
    {
        log_limited(now, "kcp dgram too short", addr, len);
        return;
    }

    AcceptEntry &e = accepting_[addr];
    e.conv         = ikcp_getconv(data);
    e.create_ms    = now;
    e.data.assign(data, (size_t)len);
}

bool KcpAcceptorIO::pop_accept(UdpAddr &addr, uint32_t &conv)
{
    std::scoped_lock sl(accept_mutex_);

    for (auto &x : accepting_)
    {
        // 已经交给业务线程了（KCP_ADD 可能还在路上），不要重复accept
        if (x.second.notified) continue;

        x.second.notified = true;
        addr              = x.first;
        conv              = x.second.conv;
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
    accepting_.erase(addr);
}

void KcpAcceptorIO::remove_accept_timeout(int64_t now)
{
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

        it = accepting_.erase(it);
    }
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
