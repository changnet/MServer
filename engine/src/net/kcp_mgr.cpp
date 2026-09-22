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
    acceptors_.clear();
}

void KcpMgr::add_acceptor(int32_t listen_id, KcpIO *acc)
{
    assert(acc);

    auto it = acceptors_.find(listen_id);
    if (it != acceptors_.end())
    {
        ELOG("KcpMgr acceptor already exist:%d", listen_id);
        it->second = acc;
        return;
    }

    acceptors_.emplace(listen_id, acc);
}

void KcpMgr::remove_acceptor(int32_t listen_id, KcpIO *acc)
{
    acceptors_.erase(listen_id);
}

void KcpMgr::remove(int32_t conn_id, bool notify_worker)
{
    auto it = establishs_.find(conn_id);
    if (it == establishs_.end()) return;

    EVIO *w   = it->second;
    KcpIO *io = static_cast<KcpIO *>(w->io_);

    // ① 释放ikcpcb（此后kcp_为nullptr，~KcpIO不会再碰它）
    if (io) io->release_kcp();

    // ② 摘"已建立表"（表在监听socket的acceptor上）
    if (io && io->listen_id() != 0)
    {
        auto a = acceptors_.find(io->listen_id());
        if (a != acceptors_.end()) a->second->unestablish(io->peer());
    }

    // ③ 摘身份表
    establishs_.erase(it);

    // ④ 通知worker（io线程主动回收时要通知，worker自己发起时不需要）
    if (notify_worker) StaticGlobal::B->dispatch_event(w, EV_CLOSE);

    // ⑤ 虚拟连接没有fd，不会走epoll关闭路径，所以这里必须自己解引用
    //    客户端形态有真实fd，M_REF_BACKEND由io线程常规关闭路径释放
    if (w->fd_ == netcompat::INVALID) w->del_ref(EVIO::M_REF_BACKEND);
}

void KcpMgr::do_add_message(ThreadMessage *m)
{
    const WatcherMsg *msg = reinterpret_cast<const WatcherMsg *>(m->buffer());

    EVIO *w              = msg->w_;
    KcpIO *io            = static_cast<KcpIO *>(w->io_);

    switch (io->get_role_type())
    {
    case IO::ACCEPTOR: break;
    case IO::LISTENER: break;
    case IO::CONNECTOR: break;
    }
    int64_t now = timing::steady_clock();

    // ① 建ikcpcb（参数来自config.hpp）
    if (!io->create_kcp(msg->conv, msg->listen_id, msg->listen_fd, msg->addr))
    {
        ELOG("kcp create fail, conn=%d", w->id_);
        StaticGlobal::B->notify_watcher(w, EV_CLOSE);
        if (w->fd_ == netcompat::INVALID) w->del_ref(EVIO::M_REF_BACKEND);
        drop_accepting(msg);
        return;
    }

    // ② ★ 必须先update一次：把updated置1并初始化ts_flush。
    //    否则 ikcp_check 恒返回 current（照它排期会死循环），ikcp_flush 也是no-op
    io->update((IUINT32)now);

    // ③ 登记身份表
    establishs_[w->id_] = w;

    /**
     * ④ ★ 服务端对端：把这条连接从accept表晋升到"已建立表"，同时取回接入
     *    窗口内缓存的数据。
     *
     *    手序很重要：必须在 create_kcp 之后、且"摘accept表 + 挂已建立表"在
     *    同一次临界区内完成。反过来做的话，中间到达的包会路由到这条连接但
     *    io->kcp() 还是 nullptr，被 input 返回 -1 误判成协议错误而回收
     */
    std::string cached;
    if (msg->listen_id != 0)
    {
        auto it = acceptors_.find(msg->listen_id);
        if (it != acceptors_.end())
        {
            it->second->promote(msg->addr, w, cached);
        }
    }

    // ⑤ 按序回放接入窗口内缓存的包（零丢包、零额外RTO）
    if (!cached.empty())
    {
        bool has_data = false;
        if (0 != io->input(cached.data(), (int32_t)cached.size(), has_data))
        {
            ELOG("kcp replay fail, conn=%d", w->id_);
            remove(w->id_, true);
            return;
        }
        if (has_data) StaticGlobal::B->notify_watcher(w, EV_READ);
    }
}

/// KCP_ADD 失败/被拒时，把accept表里对应的项删掉（否则它会一直留到16s超时）
void KcpMgr::drop_accepting(const KcpAddMsg *msg)
{
    if (0 == msg->listen_id) return;

    auto it = acceptors_.find(msg->listen_id);
    if (it != acceptors_.end()) it->second->drop_accepting(msg->addr);
}

void KcpMgr::do_del_message(ThreadMessage *m)
{
    const KcpDelMsg *msg = reinterpret_cast<const KcpDelMsg *>(m->buffer());

    /**
     * conn_id == 0：业务拒绝接入。
     * 这时候这条对端还没晋升成会话（没有socket_id），删的是accept表项
     */
    if (0 == msg->conn_id)
    {
        auto it = acceptors_.find(msg->listen_id);
        if (it != acceptors_.end()) it->second->drop_accepting(msg->addr);
        return;
    }

    /**
     * worker自己发起的关闭：
     *   客户端形态有真实fd，EV_CLOSE 会由 epoll 的关闭路径派发到worker，
     *   这里不需要再通知一次。
     *
     * ★ 虚拟连接（服务端对端，fd == -1）没有 epoll 关闭路径：
     *   stop() 里的 append_watcher_event(EV_CLOSE) 走到 modify_watcher 时，
     *   fd_mgr_.get(-1) == nullptr 会直接 return 0，EV_CLOSE 永远派发不到worker，
     *   于是 do_close 不会执行（on_disconnected 不回调、SocketMgr 里还留着对象，
     *   __socket_hash 又持有强引用，对象永远不会被GC）。
     *   所以这里必须替它把 EV_CLOSE 补上。
     */
    auto it   = establishs_.find(msg->conn_id);
    bool virt = (it != establishs_.end()) && (it->second->fd_ == netcompat::INVALID);

    remove(msg->conn_id, virt);
}

int64_t KcpMgr::update(int64_t now)
{
    // ① 每5秒扫一次accept表，回收N秒还没建立的连接
    if (now >= next_accept_timeout_)
    {
        for (auto &x : acceptors_)
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
