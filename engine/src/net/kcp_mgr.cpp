#include "kcp_mgr.hpp"

#if defined(ENABLE_KCP)

#include <vector>

#include "ev/ev_watcher.hpp"
#include "ev/time.hpp"
#include "net/io/kcp_io.hpp"
#include "system/static_global.hpp"

KcpMgr::KcpMgr()
{
}

KcpMgr::~KcpMgr()
{
    clear();
}

void KcpMgr::drop_watcher(EVIO *w)
{
    // 还没进epoll的socket：置关闭标记、通知业务线程、放掉backend引用
    w->mask_ |= EVIO::M_BACKEND_CLOSE;
    StaticGlobal::B->dispatch_event(w, EV_CLOSE);
    w->del_ref(EVIO::M_REF_BACKEND);
}

void KcpMgr::do_add_message(const WatcherMsg *m)
{
    EVIO *w   = m->w_;
    KcpIO *io = static_cast<KcpIO *>(w->io_);

    switch (io->get_role_type())
    {
    case IO::LISTENER:
    {
        if (listeners_.find(w->id_) != listeners_.end()
            || !io->on_kcpmgr_add(w))
        {
            ELOG("kcp listen fail:%d", w->id_);
            io->on_kcpmgr_remove();
            drop_watcher(w);
            return;
        }

        listeners_[w->id_] = io;

        // 监听socket有真实fd，和tcp一样注册EV_ACCEPT
        // 监听没有ikcpcb，不参与定时
        StaticGlobal::B->do_watcher_event(w, (int32_t)m->udata_, false);
        return;
    }
    case IO::ACCEPTOR:
    {
        auto it = listeners_.find(io->get_main_id());
        if (it == listeners_.end())
        {
            ELOG("kcp accept listener not found:%d %d", w->id_,
                 io->get_main_id());
            drop_watcher(w);
            return;
        }

        if (!it->second->establish_accept(w))
        {
            ELOG("kcp accept fail:%d", w->id_);
            io->on_kcpmgr_remove();
            drop_watcher(w);
            return;
        }

        establishs_[w->id_] = w;

        // 必须先update一次：把updated置1并初始化ts_flush。
        // 否则ikcp_flush是no-op（一包都发不出去），ikcp_check恒返回current（照它排期会空转）
        io->update(timing::steady_clock());
        return;
    }
    case IO::CONNECTOR:
    {
        if (establishs_.find(w->id_) != establishs_.end() || !io->on_kcpmgr_add(w))
        {
            ELOG("kcp connect fail:%d", w->id_);
            io->on_kcpmgr_remove();
            drop_watcher(w);
            return;
        }

        establishs_[w->id_] = w;

        // 客户端的真实fd只关心可读：kcp没有connect事件
        StaticGlobal::B->do_watcher_event(w, (int32_t)m->udata_, false);

        // 同ACCEPTOR：这次update是ikcp_flush和ikcp_check能正常工作的前提
        io->update(timing::steady_clock());
        return;
    }
    }
}

void KcpMgr::remove(EVIO *w, bool flush)
{
    KcpIO *io = static_cast<KcpIO *>(w->io_);

    // 监听socket：没有ikcpcb，关闭时把它名下的对端一并回收
    if (IO::LISTENER == io->get_role_type())
    {
        if (0 == listeners_.erase(w->id_)) return;

        std::vector<EVIO *> peers = io->peers();
        for (EVIO *p : peers) remove(p, flush);

        io->on_kcpmgr_remove();

        // 监听有真实fd，走和tcp一样的事件
        StaticGlobal::B->do_watcher_event(w, EV_CLOSE, false);
        return;
    }

    if (0 == establishs_.erase(w->id_)) return; // 已经关过了

    if (IO::ACCEPTOR == io->get_role_type())
    {
        auto it = listeners_.find(io->get_main_id());
        if (it != listeners_.end()) it->second->remove_establish(w);
    }

    // flush语义：剩余帧交给kcp并flush一次，然后立刻释放（kcp没有优雅关闭）
    if (flush) io->flush(w);
    io->on_kcpmgr_remove();

    if (netcompat::INVALID == w->fd_)
    {
        // 虚拟连接不在fd_mgr_里，epoll无关，只能自己完成关闭
        drop_watcher(w);
    }
    else
    {
        // 真实fd：摘epoll、派发EV_CLOSE、放backend引用
        StaticGlobal::B->do_watcher_event(w, EV_CLOSE, false);
    }
}

void KcpMgr::do_del_message(const WatcherMsg *m)
{
    remove(m->w_, 0 != m->udata_);
}

int64_t KcpMgr::update(int64_t now)
{
    // ① 每N秒扫一次accept表，回收还没接入的对端
    if (now >= next_accept_timeout_)
    {
        for (auto &x : listeners_)
        {
            x.second->remove_accept_timeout(now);
        }
        next_accept_timeout_ = now + KCP_ACCEPT_SCAN_MS;
    }

    /**
     * ② 遍历所有会话，执行ikcp_update，并计算下一次最小执行时间
     *
     * TODO 当数量比较多时，直接遍历cpu占用会比较高，后续估计要用时间分片
     * 或者树型结构，或者模仿内核epoll的机制来优化
     */
    int64_t next = KCP_ACCEPT_SCAN_MS;

    for (auto &x : establishs_)
    {
        KcpIO *io = static_cast<KcpIO *>(x.second->io_);
        int64_t t = io->update(now);

        if (t < next) next = t;
    }

    return next;
}

void KcpMgr::clear()
{
    for (auto &x : establishs_)
    {
        EVIO *w   = x.second;
        KcpIO *io = static_cast<KcpIO *>(w->io_);

        io->on_kcpmgr_remove();

        // 虚拟连接不在fd_mgr_里，backend引用只能自己放
        if (netcompat::INVALID == w->fd_) w->del_ref(EVIO::M_REF_BACKEND);
    }
    establishs_.clear();

    for (auto &x : listeners_) x.second->on_kcpmgr_remove();
    listeners_.clear();
}

#endif
