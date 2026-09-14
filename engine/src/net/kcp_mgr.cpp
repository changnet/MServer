#include "kcp_mgr.hpp"

#if defined(ENABLE_KCP)

#include <string>

#include "ev/ev_watcher.hpp"
#include "ev/time.hpp"
#include "net/io/kcp_acceptor_io.hpp"
#include "net/io/kcp_io.hpp"
#include "system/static_global.hpp"

/// accept表里没有待接入对端时的回收节拍（有对端时才会真正被用到）
static constexpr int64_t KCP_SWEEP_INTERVAL = 1000;

KcpMgr::~KcpMgr()
{
    // io线程已经join，这里独占访问。
    // ikcpcb 必须在这里放掉，否则 ~KcpIO 会在worker线程碰它
    for (auto &x : conns_)
    {
        EVIO *w   = x.second;
        KcpIO *io = static_cast<KcpIO *>(w->io_);
        if (io) io->release_kcp();

        // 虚拟连接不在 fd_mgr_ 里，io线程不会走epoll关闭路径替它解引用
        if (w->fd_ == netcompat::INVALID) w->del_ref(EVIO::M_REF_BACKEND);
    }

    conns_.clear();
    conn_ids_.clear();
    acceptors_.clear();
}

void KcpMgr::reg_acceptor(int32_t listen_id, KcpAcceptorIO *acc)
{
    assert(acc);
    acceptors_[listen_id] = acc;
}

void KcpMgr::unreg_acceptor(int32_t listen_id)
{
    acceptors_.erase(listen_id);
}

void KcpMgr::remove(int32_t conn_id, bool notify_worker)
{
    auto it = conns_.find(conn_id);
    if (it == conns_.end()) return;

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

    // ③ 摘身份表 + 迭代视图（swap-erase，O(1)，不要缓存迭代器）
    conns_.erase(it);
    for (size_t i = 0; i < conn_ids_.size(); ++i)
    {
        if (conn_ids_[i] == conn_id)
        {
            conn_ids_[i] = conn_ids_.back();
            conn_ids_.pop_back();
            if (cursor_ > i) --cursor_;
            break;
        }
    }

    // ④ 通知worker（io线程主动回收时要通知，worker自己发起时不需要）
    if (notify_worker) StaticGlobal::B->notify_watcher(w, EV_CLOSE);

    // ⑤ 虚拟连接没有fd，不会走epoll关闭路径，所以这里必须自己解引用
    //    客户端形态有真实fd，M_REF_BACKEND由io线程常规关闭路径释放
    if (w->fd_ == netcompat::INVALID) w->del_ref(EVIO::M_REF_BACKEND);
}

void KcpMgr::on_add(ThreadMessage *m)
{
    const KcpAddMsg *msg = reinterpret_cast<const KcpAddMsg *>(m->buffer());
    EVIO *w              = msg->conn_w;
    KcpIO *io            = w ? static_cast<KcpIO *>(w->io_) : nullptr;

    if (!io)
    {
        // 脚本报错导致io没建出来。这条连接io线程不会再管，必须让worker关掉，
        // 否则 M_REF_BACKEND 永远放不掉
        ELOG("kcp add no io, conn=%d", w ? w->id_ : -1);
        if (w)
        {
            StaticGlobal::B->notify_watcher(w, EV_CLOSE);
            if (w->fd_ == netcompat::INVALID) w->del_ref(EVIO::M_REF_BACKEND);
        }
        drop_accepting(msg);
        return;
    }

    if (conns_.size() >= (size_t)KCP_MAX_SESSION)
    {
        PLOG("kcp session full, drop conn=%d", w->id_);

        // 会话已满：不建ikcpcb，直接让worker关掉这条连接。
        // worker收到EV_CLOSE后走do_close → stop() → 又递一条KCP_DEL，
        // 那时conns_里没有它，remove()直接返回，幂等。
        StaticGlobal::B->notify_watcher(w, EV_CLOSE);

        // 虚拟连接没有fd，不会走epoll关闭路径，这里自己解引用（规则同remove()）
        if (w->fd_ == netcompat::INVALID) w->del_ref(EVIO::M_REF_BACKEND);

        drop_accepting(msg);
        return;
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
    ikcp_update(io->kcp(), (IUINT32)now);
    io->touch(now);

    // ③ 登记身份表 + 迭代视图
    conns_[w->id_] = w;
    conn_ids_.push_back(w->id_);

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

    // ⑥ 有新会话了，把全局节拍拉近
    if (next_update_ == 0 || now + KCP_INTERVAL < next_update_)
        next_update_ = now + KCP_INTERVAL;
}

/// KCP_ADD 失败/被拒时，把accept表里对应的项删掉（否则它会一直留到16s超时）
void KcpMgr::drop_accepting(const KcpAddMsg *msg)
{
    if (0 == msg->listen_id) return;

    auto it = acceptors_.find(msg->listen_id);
    if (it != acceptors_.end()) it->second->drop_accepting(msg->addr);
}

void KcpMgr::on_del(ThreadMessage *m)
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
     *   stop() 里的 add_watcher_event(EV_CLOSE) 走到 modify_watcher 时，
     *   fd_mgr_.get(-1) == nullptr 会直接 return 0，EV_CLOSE 永远派发不到worker，
     *   于是 do_close 不会执行（on_disconnected 不回调、SocketMgr 里还留着对象，
     *   __socket_hash 又持有强引用，对象永远不会被GC）。
     *   所以这里必须替它把 EV_CLOSE 补上。
     */
    auto it   = conns_.find(msg->conn_id);
    bool virt = (it != conns_.end()) && (it->second->fd_ == netcompat::INVALID);

    remove(msg->conn_id, virt);
}

int64_t KcpMgr::update(int64_t now)
{
    /**
     * ① 定期回收accept表里16s还没晋升的对端。
     *    acceptor 内部有原子短路（没有待接入对端时立刻返回），
     *    所以放在每拍执行的开销可以忽略
     */
    bool pending = false;
    for (auto &x : acceptors_)
    {
        if (x.second->sweep(now)) pending = true;
    }

    if (conns_.empty())
    {
        // accept表里还有等待接入的对端 → 每秒回来做一次超时回收
        next_update_ = pending ? (now + KCP_SWEEP_INTERVAL) : 0;
        return pending ? KCP_SWEEP_INTERVAL : -1;
    }

    // 还没到点，直接返回剩余时间（io线程用它收紧wait超时）
    if (now < next_update_) return next_update_ - now;

    bool more = tick(now);

    // 这一拍没处理完 → 立刻再来一轮，不要白等40ms。
    // 节拍长度天然被 IKCP_INTERVAL 限制（ikcp_check 的返回值上限就是interval）
    next_update_ = more ? now : now + KCP_INTERVAL;

    return next_update_ - now;
}

bool KcpMgr::tick(int64_t now)
{
    IUINT32 now32 = (IUINT32)now;

    size_t total = conn_ids_.size();
    if (0 == total) return false;

    size_t n    = total < (size_t)KCP_TICK_BATCH ? total
                                                : (size_t)KCP_TICK_BATCH;
    size_t done = 0;

    while (done < n)
    {
        if (cursor_ >= conn_ids_.size()) cursor_ = 0;

        int32_t conn_id = conn_ids_[cursor_];

        // ★ 分片期间 conn_ids_ 可能被 remove() 改动（swap-erase会把尾部元素
        //   换到当前下标），所以每次都要重新取，并且不要缓存迭代器
        if (++done > conn_ids_.size()) break; // 防御：一轮内被删空
        if (cursor_ < conn_ids_.size() && conn_ids_[cursor_] == conn_id)
            ++cursor_;

        auto it = conns_.find(conn_id);
        if (it == conns_.end()) continue;

        KcpIO *io = static_cast<KcpIO *>(it->second->io_);
        if (!io || !io->kcp()) continue;

        // 这一句把重传 / ACK / 探测包发出去（内部会sendto）
        ikcp_update(io->kcp(), now32);
    }

    return n < total;
}

void KcpMgr::touch(int32_t conn_id)
{
    auto it = conns_.find(conn_id);
    if (it == conns_.end()) return;

    KcpIO *io = static_cast<KcpIO *>(it->second->io_);
    if (io) io->touch(timing::steady_clock());
}

#endif
