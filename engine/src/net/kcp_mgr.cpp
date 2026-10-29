#include "kcp_mgr.hpp"

#include "ev/time.hpp"
#include "ev/ev_watcher.hpp"
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
    // 还没进epoll的socket：先置关闭标记，业务线程收到EV_CLOSE才能去关fd
    w->mask_ |= EVIO::M_BACKEND_CLOSE;
    StaticGlobal::B->dispatch_event(w, EV_CLOSE);

    /**
     * 不能直接 del_ref：remove_watcher 把"放引用"和"清理消息队列里同一条w的消息"
     * 放在同一把锁里，业务线程入队侧也在锁里判 M_REF_BACKEND，两者才不会交错。
     * ACCEPTOR 是虚拟连接，fd_ 为 INVALID，对 fd_mgr_ 是空操作
     */
    StaticGlobal::B->remove_watcher(w, w->fd_);
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

        /**
         * 这里不需要先手动update一次。ikcp_check在updated==0时返回current，
         * KcpIO::update看到(t - now) <= 0就会补一次ikcp_update，第一次flush
         * 随即发生。而do_thread_message处理完消息后紧接着就是下一轮主循环的
         * KcpMgr::update，中间不做任何等待，所以这里少调一次不影响首次发送时机
         */
        establishs_[w->id_] = w;
        // kcp建立后必须update过一次ikcp_flush才会发送数据
        // 这里不update只能等下次循环，期间启用了NODEALY的在send里的flush不生效
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
        // 建立后的第一次flush同样交给主循环的 KcpMgr::update，理由见ACCEPTOR分支
        StaticGlobal::B->do_watcher_event(w, (int32_t)m->udata_, false);

        io->update(timing::steady_clock());
        return;
    }
    }
}

void KcpMgr::remove(EVIO *w, bool flush)
{
    KcpIO *io          = static_cast<KcpIO *>(w->io_);
    const int32_t role = io->get_role_type();

    // 监听socket：没有ikcpcb，关闭时把它名下的对端一并回收
    if (IO::LISTENER == role)
    {
        if (0 == listeners_.erase(w->id_)) return;

        io->remove_establish_peers(flush);
        io->on_kcpmgr_remove();

        // 监听有真实fd，走和tcp一样的事件
        StaticGlobal::B->do_watcher_event(w, EV_CLOSE, false);
        return;
    }

    if (0 == establishs_.erase(w->id_)) return; // 已经关过了

    if (IO::ACCEPTOR == role)
    {
        auto it = listeners_.find(io->get_main_id());
        if (it != listeners_.end()) it->second->remove_establish(w);
    }

    // flush语义：剩余帧交给kcp并flush一次，然后立刻释放（kcp没有优雅关闭）
    if (flush) io->flush(w);
    io->on_kcpmgr_remove();

    // ACCEPTOR没有内核对象，epoll和fd_mgr_里都没有它，只能自己完成关闭
    if (IO::ACCEPTOR == role)
    {
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
    remove(m->w_, 0 != (m->udata_ & EV_FLUSH));
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
        EVIO *w            = x.second;
        KcpIO *io          = static_cast<KcpIO *>(w->io_);
        const int32_t role = io->get_role_type();

        io->on_kcpmgr_remove();

        // ACCEPTOR是虚拟连接，不在fd_mgr_里，backend引用只能自己放
        if (IO::ACCEPTOR == role) w->del_ref(EVIO::M_REF_BACKEND);
    }
    establishs_.clear();

    for (auto &x : listeners_) x.second->on_kcpmgr_remove();
    listeners_.clear();
}
