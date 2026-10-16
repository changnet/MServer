#pragma once

#include <unordered_map>

#include "global/global.hpp"

#include "net/udp_addr.hpp"
#include "thread/thread_context.hpp"

#if defined(ENABLE_KCP)

class EVIO;
class KcpIO;

/**
 * kcp 会话管理器（io线程独占，单例挂在 EVBackend 上）
 *
 * 只保留"身份表"：
 *   establishs_     socket_id → 连接 EVIO（身份一律用 socket_id）
 *
 * 路由（listen_id + 源地址 → socket_id）和"接入窗口缓存"都已经下沉到
 * 各自的 KcpAcceptorIO 上 —— 因为一个 kcp 监听 fd 上承载 N 个对端，只有
 * 监听socket自己才知道这些对端属于它，放在全局扁平表里会让两个监听socket
 * 上的同一个客户端地址互相覆盖。
 *
 * 定时：不用 multimap，也不做游标分片。每轮 update() 老老实实遍历一次
 * conns_，对每个会话问 ikcp_check "你下次想什么时候被处理"，取最小值作为
 * 下一轮主循环的 wait 超时。这样每个会话都是在自己的 ts_flush 到点的那一
 * 轮被 flush，相位零偏移（固定节拍就不行：ts_flush 的相位取决于会话创建
 * 时刻，与固定节拍点存在恒定偏移）。
 */
class KcpMgr
{
public:
    KcpMgr();
    ~KcpMgr();

    void do_add_message(const WatcherMsg *m);    // KCP_ADD
    void do_del_message(const WatcherMsg *m); // KCP_DEL

    /// 移除一个kcp连接
    void remove(EVIO *w, int32_t event);

    /**
     * @brief 驱动所有会话的定时器，返回下一轮主循环建议的wait超时(ms)
     *
     * 到点的会话当场 ikcp_update()（把重传/ACK/窗口探测发出去），
     * 没到点的会话用 ikcp_check() 算出还剩多少毫秒，取全局最小值返回。
     *
     * @return >= 0：下一轮wait的超时毫秒数；-1：没有任何定时在跑，不需要被唤醒
     */
    int64_t update(int64_t now);

private:

    /// 已建立的连接：socket_id → 连接 EVIO
    std::unordered_map<int32_t, EVIO *> establishs_;

    /// 监听的socket，以socket_id为key
    std::unordered_map<int32_t, KcpIO *> listeners_;

    int64_t next_accept_timeout_ = 0; // 下一次遍历acceptor回收超时连接时间戳(ms)
};

#endif
