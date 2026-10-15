#include "kcp_mgr.hpp"

#if defined(ENABLE_KCP)

#include <string>

#include "ev/ev_watcher.hpp"
#include "ev/time.hpp"
#include "net/io/kcp_io.hpp"
#include "system/static_global.hpp"

/// accept表里对端的回收扫描间隔（超时本身是16s，5s的粒度足够）
static constexpr int64_t KCP_ACCEPT_TIMEOUT = 5000;

KcpMgr::~KcpMgr()
{
    // io线程已经join，这里独占访问。
    // ikcpcb 必须在这里放掉，否则 ~KcpIO 会在worker线程碰它
    for (auto &x : establishs_)
    {
        EVIO *w   = x.second;
        KcpIO *io = static_cast<KcpIO *>(w->io_);
        if (io) io->release_kcp();

        // 虚拟连接不在 fd_mgr_ 里，io线程不会走epoll关闭路径替它解引用
        if (w->fd_ == netcompat::INVALID) w->del_ref(EVIO::M_REF_BACKEND);
    }

    establishs_.clear();
    listeners_.clear();
}

void KcpMgr::do_add_message(ThreadMessage *m)
{
    const WatcherMsg *msg = reinterpret_cast<const WatcherMsg *>(m->buffer());

    EVIO *w              = msg->w_;
    KcpIO *io            = static_cast<KcpIO *>(w->io_);

    switch (io->get_role_type())
    {
    case IO::ACCEPTOR:
    {
        auto eit = establishs_.find(w->id_);
        if (eit != establishs_.end())
        {
            ELOG("kcp accept already exist:%d", w->id_);
            StaticGlobal::B->dispatch_event(w, EV_CLOSE);
            return;
        }
        int64_t main_id = io->get_main_id();
        auto it = listeners_.find(main_id);
        if (it != listeners_.end())
        {
            ELOG("kcp accept listener not found:%d %d", w->id_, main_id);
            StaticGlobal::B->dispatch_event(w, EV_CLOSE);
            return;
        }

        if (!io->establish_accept(w))
        {
            StaticGlobal::B->dispatch_event(w, EV_CLOSE);
            return;
        }

        establishs_[w->id_] = w;
        // udp的acceptor不参与读写事件循环，由监听socket负责
        // StaticGlobal::B->do_watcher_event(w, msg->udata_, false);
        break;
    }
    case IO::LISTENER:
    {
        auto it = listeners_.find(w->id_);
        if (it != listeners_.end())
        {
            ELOG("kcp listen already exist:%d", w->id_);
            StaticGlobal::B->dispatch_event(w, EV_CLOSE);
            return;
        }
        if (!io->on_kcpmgr_add(w))
        {
            StaticGlobal::B->dispatch_event(w, EV_CLOSE);
            return;
        }

        listeners_[w->id_] = io;
        establishs_[w->id_] = w;
        StaticGlobal::B->do_watcher_event(w, msg->udata_, false);
        break;
    }
    case IO::CONNECTOR:
    {
        auto it = establishs_.find(w->id_);
        if (it != establishs_.end())
        {
            ELOG("kcp connect already exist:%d", w->id_);
            StaticGlobal::B->dispatch_event(w, EV_CLOSE);
            return;
        }
        if (!io->on_kcpmgr_add(w))
        {
            StaticGlobal::B->dispatch_event(w, EV_CLOSE);
            return;
        }

        establishs_[w->id_] = w;
        StaticGlobal::B->do_watcher_event(w, msg->udata_, false);
        break;
    }
    }

    int64_t now = timing::steady_clock();

    // 必须先update一次：把updated置1并初始化ts_flush。
    // 否则 ikcp_check 恒返回 current（照它排期会死循环），ikcp_flush 也是no-op
    io->update((IUINT32)now);
}

void KcpMgr::remove(EVIO *w, int32_t event)
{
    KcpIO *io = static_cast<KcpIO *>(w->io_);

    switch (io->get_role_type())
    {
    case IO::ACCEPTOR:
    {
        auto eit = establishs_.find(w->id_);
        if (eit == establishs_.end())
        {
            ELOG("kcp accept not exist:%d", w->id_);
            StaticGlobal::B->dispatch_event(w, EV_CLOSE);
            return;
        }
        int64_t main_id = io->get_main_id();
        auto it         = listeners_.find(main_id);
        if (it != listeners_.end())
        {
            ELOG("kcp accept listener not found:%d %d", w->id_, main_id);
            StaticGlobal::B->dispatch_event(w, EV_CLOSE);
            return;
        }

        establishs_[w->id_] = w;
        io->remove_establish(w);

        StaticGlobal::B->dispatch_event(w, EV_CLOSE);
        break;
    }
    case IO::LISTENER:
    {
        auto it = listeners_.find(w->id_);
        if (it == listeners_.end())
        {
            ELOG("kcp listen not exist:%d", w->id_);
            StaticGlobal::B->dispatch_event(w, EV_CLOSE);
            return;
        }

        listeners_[w->id_]  = io;
        establishs_[w->id_] = w;

        // 执行close事件，从epoll等移除。可能包含EV_FLUSH事件，必须先执行
        StaticGlobal::B->do_watcher_event(w, event, false);

        // 这个不能在这里执行，因为EV_FLUSH还需要kcp的结构
        // 而EV_FLUSH执行完后主线程可能
        // io->on_kcpmgr_remove(w);

        break;
    }
    case IO::CONNECTOR:
    {
        auto it = establishs_.find(w->id_);
        if (it != establishs_.end())
        {
            ELOG("kcp connect already exist:%d", w->id_);
            StaticGlobal::B->dispatch_event(w, EV_CLOSE);
            return;
        }

        // 同样不能在外网执行
        // io->on_kcpmgr_remove(w);

        establishs_[w->id_] = w;
        StaticGlobal::B->do_watcher_event(w, event, false);
        break;
    }
    }
}

void KcpMgr::do_del_message(ThreadMessage *m)
{
    const WatcherMsg *msg = reinterpret_cast<const WatcherMsg *>(m->buffer());

    remove(msg->w_, msg->udata_);
}

int64_t KcpMgr::update(int64_t now)
{
    // ① 每5秒扫一次accept表，回收N秒还没建立的连接
    if (now >= next_accept_timeout_)
    {
        for (auto &x : listeners_)
        {
            x.second->remove_accept_timeout(now);
        }
        next_accept_timeout_ = now + KCP_ACCEPT_TIMEOUT;
    }

    /**
     * ② 遍历所有会话，执行ikcp_update，并计算下一次最小执行的时间
     * 
     * TODO 当数量比较多时，直接遍历cpu占用会比较高，后续估计要用时间分片
     * 或者树型结构，或者模仿内核epoll的机制来优化
     */
    const IUINT32 cur = (IUINT32)now;
    int64_t next      = now + KCP_ACCEPT_TIMEOUT;

    for (auto &x : establishs_)
    {
        KcpIO *io = static_cast<KcpIO *>(x.second->io_);
        int64_t t = io->update(now);

        if (t < next) next = t;
    }

    return next;
}

#endif
