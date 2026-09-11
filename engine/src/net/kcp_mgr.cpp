#include "kcp_mgr.hpp"

#if defined(ENABLE_KCP)

#include "ev/ev_watcher.hpp"
#include "ev/time.hpp"
#include "net/io/kcp_io.hpp"
#include "net/kcp_msg.hpp"
#include "system/static_global.hpp"

KcpMgr::~KcpMgr()
{
    // backend线程已经join，这里独占访问。
    // ikcpcb 必须在这里放掉，否则 ~KcpIO 会在worker线程碰它
    for (auto &x : conns_)
    {
        EVIO *w   = x.second;
        KcpIO *io = static_cast<KcpIO *>(w->io_);
        if (io) io->release_kcp();

        // 虚拟连接不在 fd_mgr_ 里，backend不会走epoll关闭路径替它解引用
        if (w->fd_ == netcompat::INVALID) w->del_ref(EVIO::M_REF_BACKEND);
    }

    conns_.clear();
    route_.clear();
    parked_.clear();
    conn_ids_.clear();
}

EVIO *KcpMgr::route(int32_t listen_id, const UdpAddr &addr) const
{
    auto it = route_.find(listen_id);
    if (it == route_.end()) return nullptr;

    auto it2 = it->second.find(addr);
    if (it2 == it->second.end()) return nullptr;

    auto it3 = conns_.find(it2->second);
    return it3 == conns_.end() ? nullptr : it3->second;
}

bool KcpMgr::park(int32_t listen_id, int32_t listen_fd, int32_t worker_addr,
                  const UdpAddr &addr, uint32_t conv, const char *data,
                  int32_t len)
{
    if (parked_.size() >= (size_t)KCP_MAX_PARKED) return false;

    Parked &p   = parked_[addr];
    p.listen_id = listen_id;
    p.listen_fd = listen_fd;
    p.conv      = conv;
    p.data.assign(data, (size_t)len);

    KcpAcceptMsg msg;
    msg.listen_id = listen_id;
    msg.conv      = conv;
    msg.addr      = addr;

    // 走现成的forward_message，不新造投递函数
    return StaticGlobal::M->forward_message(0, worker_addr,
                                            ThreadMessage::KCP_ACCEPT, &msg,
                                            (int32_t)sizeof(msg));
}

bool KcpMgr::append_parked(const UdpAddr &addr, const char *data, int32_t len)
{
    auto it = parked_.find(addr);
    if (it == parked_.end()) return false;

    // 接入窗口内又来包：追加到已park的数据后面，KCP_ADD时按序回放
    // （零丢包、零额外RTO）。但必须封顶，否则addr洪水能撑爆这个string
    if (it->second.data.size() + (size_t)len > (size_t)KCP_MAX_PARKED_DATA)
    {
        ++stat_drop_unknown_;
        return true; // 已"吞掉"这一包，只是丢弃
    }

    it->second.data.append(data, (size_t)len);
    return true;
}

void KcpMgr::remove(int32_t conn_id, bool notify_worker)
{
    auto it = conns_.find(conn_id);
    if (it == conns_.end()) return;

    EVIO *w   = it->second;
    KcpIO *io = static_cast<KcpIO *>(w->io_);

    // ① 释放ikcpcb（此后kcp_为nullptr，~KcpIO不会再碰它）
    if (io) io->release_kcp();

    // ② 摘反向索引
    if (io && io->listen_id() != 0)
    {
        auto r = route_.find(io->listen_id());
        if (r != route_.end())
        {
            r->second.erase(io->peer());
            if (r->second.empty()) route_.erase(r);
        }
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

    // ④ 通知worker（backend主动回收时要通知，worker自己发起时不需要）
    if (notify_worker) StaticGlobal::B->notify_watcher(w, EV_CLOSE);

    // ⑤ 虚拟连接没有fd，不会走epoll关闭路径，所以这里必须自己解引用
    //    客户端形态有真实fd，M_REF_BACKEND由backend常规关闭路径释放
    if (w->fd_ == netcompat::INVALID) w->del_ref(EVIO::M_REF_BACKEND);
}

void KcpMgr::on_add(ThreadMessage *m)
{
    const KcpAddMsg *msg = reinterpret_cast<const KcpAddMsg *>(m->buffer());
    EVIO *w              = msg->conn_w;
    KcpIO *io            = w ? static_cast<KcpIO *>(w->io_) : nullptr;

    if (!io)
    {
        // 脚本报错导致io没建出来。这条连接backend不会再管，必须让worker关掉，
        // 否则 M_REF_BACKEND 永远放不掉
        ELOG("kcp add no io, conn=%d", w ? w->id_ : -1);
        if (w)
        {
            StaticGlobal::B->notify_watcher(w, EV_CLOSE);
            if (w->fd_ == netcompat::INVALID) w->del_ref(EVIO::M_REF_BACKEND);
        }
        parked_.erase(msg->addr);
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

        parked_.erase(msg->addr);
        return;
    }

    int64_t now = timing::steady_clock();

    // ① 建ikcpcb（参数来自config.hpp）
    if (!io->create_kcp(msg->conv, msg->listen_id, msg->listen_fd, msg->addr))
    {
        ELOG("kcp create fail, conn=%d", w->id_);
        StaticGlobal::B->notify_watcher(w, EV_CLOSE);
        if (w->fd_ == netcompat::INVALID) w->del_ref(EVIO::M_REF_BACKEND);
        parked_.erase(msg->addr);
        return;
    }

    // ② ★ 必须先update一次：把updated置1并初始化ts_flush。
    //    否则 ikcp_check 恒返回 current（照它排期会死循环），ikcp_flush 也是no-op
    ikcp_update(io->kcp(), (IUINT32)now);
    io->touch(now);

    // ③ 登记身份表 + 迭代视图
    conns_[w->id_] = w;
    conn_ids_.push_back(w->id_);

    // ④ 服务端对端：登记反向索引
    if (msg->listen_id != 0)
    {
        route_[msg->listen_id][msg->addr] = w->id_;
    }

    // ⑤ ★ 回放接入窗口内park的包（零丢包、零额外RTO）
    //    注意先删掉parked_再input：input内部只可能发ACK，不会重入本表
    bool has_data = false;
    auto p        = parked_.find(msg->addr);
    if (p != parked_.end())
    {
        io->input(p->second.data.data(), (int32_t)p->second.data.size(),
                  has_data);
        parked_.erase(p);
    }

    if (has_data) StaticGlobal::B->notify_watcher(w, EV_READ);

    // ⑥ 有新会话了，把全局节拍拉近
    if (next_update_ == 0 || now + KCP_INTERVAL < next_update_)
        next_update_ = now + KCP_INTERVAL;
}

void KcpMgr::on_del(ThreadMessage *m)
{
    const KcpDelMsg *msg = reinterpret_cast<const KcpDelMsg *>(m->buffer());

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
    if (conns_.empty())
    {
        next_update_ = 0;
        return -1; // 没有会话，不需要被唤醒
    }

    // 还没到点，直接返回剩余时间（backend用它收紧wait超时）
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
