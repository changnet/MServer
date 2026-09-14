#include "kcp_mgr.hpp"

#if defined(ENABLE_KCP)

#include <string>

#include "ev/ev_watcher.hpp"
#include "ev/time.hpp"
#include "net/io/kcp_acceptor_io.hpp"
#include "net/io/kcp_io.hpp"
#include "system/static_global.hpp"

/// accept表里对端的回收扫描间隔（超时本身是16s，5s的粒度足够）
static constexpr int64_t KCP_SWEEP_INTERVAL = 5000;

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
    acceptors_.clear();
}

void KcpMgr::reg_acceptor(int32_t listen_id, KcpAcceptorIO *acc)
{
    assert(acc);

    // 幂等：同名重复登记（KcpAcceptorIO::accept 每次都会调）直接覆盖。
    // 监听socket关闭后重建时，新acceptor会覆盖掉旧的悬空指针
    auto it = acceptors_.find(listen_id);
    if (it != acceptors_.end())
    {
        it->second = acc;
        return;
    }

    acceptors_.emplace(listen_id, acc);
}

void KcpMgr::unreg_acceptor(KcpAcceptorIO *acc)
{
    // 监听socket数量极少（通常1~2个），按值反查即可
    for (auto it = acceptors_.begin(); it != acceptors_.end(); ++it)
    {
        if (it->second != acc) continue;

        acceptors_.erase(it);
        return;
    }
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

    // ③ 摘身份表
    conns_.erase(it);

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

    // ③ 登记身份表
    conns_[w->id_] = w;

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
    // ① 每5秒扫一次accept表，回收N秒还没建立的连接
    if (now >= next_sweep_)
    {
        for (auto &x : acceptors_)
        {
            x.second->sweep(now);
        }
        next_sweep_ = now + KCP_SWEEP_INTERVAL;
    }

    /**
     * ② 遍历所有会话，问 ikcp_check "你下次想什么时候被处理"，取最小值
     *    作为下一轮主循环的wait超时。
     *
     *    官方 ikcp_check（ikcp.c:1275）只有三种返回：
     *      ① updated==0 / current 已到或过了 ts_flush / snd_buf 里有段的
     *         resendts 已到 → 返回 current（"现在就该动"）
     *      ② 否则返回 current + min(到下个 flush 点, 到最近一个段的重传点)
     *      ③ 上面的差值被 `if (minimal >= kcp->interval) minimal = kcp->interval;` 夹住
     *    → 返回值的上限就是 current + interval。**这个 40ms 是 ikcp 自己的设计
     *      上限，不是我们选的节拍**：一旦有包该发（重传到点、ts_flush 到点），
     *      ikcp_check 会明确返回 current 让我们立刻动，不会让重传等到下一拍。
     *
     *    ikcp_update 的签名是 void（官方如此），所以 flush 完必须自己再 check
     *    一次才能拿到新的下次时间。
     */
    const IUINT32 cur = (IUINT32)now;
    int64_t next      = -1; // -1 = 还没有

    for (auto &x : conns_)
    {
        KcpIO *io = static_cast<KcpIO *>(x.second->io_);
        if (!io) continue;

        IKCPCB *kcp = io->kcp();
        if (!kcp) continue;

        IUINT32 t = ikcp_check(kcp, cur);
        if (t == cur)
        {
            // 到点了：该重传的、该发 ACK 的、窗口探测全在这里发出去
            ikcp_update(kcp, cur);
            t = ikcp_check(kcp, cur);

            /**
             * ★ 补一条：ikcp_flush 只在 `slap = cur - ts_flush >= 0` 时才真正
             *   执行（ikcp.c:1256），而 ikcp_check 在"有段已过 resendts"时也会
             *   返回 current。两者一起出现的场景是：某个段的重传点落在两个
             *   flush 点之间（resendts 由 rto 决定，而 rto 通常不是 interval
             *   的整数倍，比如 rto=70 或退避后的 300）。
             *   这时 update 是空操作、再 check 仍返回 current —— 如果照它给 0，
             *   主循环就会以 min_wait(1ms) 空转到下一个 flush 点。
             *   既然这一段本来也要等到 ts_flush 才能发出去，就直接把下次唤醒
             *   对齐到 ts_flush，省掉这段空转（重传时刻不变）
             */
            if (t == cur)
            {
                IINT32 to_flush = (IINT32)(kcp->ts_flush - cur);
                if (to_flush > 0) t = kcp->ts_flush;
            }
        }

        // 和 ikcp.c 的 _itimediff 一个语义：有符号差值，天然处理 IUINT32 回绕
        int64_t d = (int64_t)(IINT32)(t - cur);
        if (d < 0) d = 0;
        if (next < 0 || d < next) next = d;
    }

    // ③ accept表的回收不重要，就不管了。什么时候调用update什么时候检测就行

    return next;
}

#endif
