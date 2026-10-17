#pragma once

#include <unordered_map>

#include "global/global.hpp"

#include "net/udp_addr.hpp"
#include "thread/thread_context.hpp"

#if defined(ENABLE_KCP)

class EVIO;
class KcpIO;

/**
 * kcp 会话管理器（backend线程独占，挂在 EVBackend 上）
 *
 * 两张身份表，都以 socket_id 为key：
 *   establishs_  socket_id → 会话 EVIO（客户端 + 服务端对端）
 *   listeners_   socket_id → 监听 socket 的 KcpIO
 *
 * 路由（源地址 → 会话）和接入缓存不在这里：一个监听fd上承载N个对端，
 * 只有监听socket自己知道这些对端属于它，所以那张表挂在它的 KcpIO 上。
 *
 * 定时不做分片：每轮遍历 establishs_，问 ikcp_check 下次什么时候需要被处理，
 * 取最小值作为主循环 wait 的超时（相对毫秒）。
 */
class KcpMgr
{
public:
    KcpMgr();
    ~KcpMgr();

    void do_add_message(const WatcherMsg *m); // KCP_ADD
    void do_del_message(const WatcherMsg *m); // KCP_DEL

    /**
     * @brief 关闭一个kcp socket（backend线程）
     * @param flush 关闭前尽力把发送缓冲区的数据flush出去（kcp不保证送达）
     */
    void remove(EVIO *w, bool flush);

    /**
     * @brief 驱动所有会话的定时器
     * @return 距离下次需要被处理的毫秒数（相对值，可直接当wait超时用）
     */
    int64_t update(int64_t now);

    // 释放所有kcp独占的数据（退出时由 EVBackend 在清fd引用之前调用）
    void clear();

private:
    // 通知业务线程回收，用于还没注册到epoll的socket
    void drop_watcher(EVIO *w);

    /// 已建立的会话：socket_id → 会话 EVIO
    std::unordered_map<int32_t, EVIO *> establishs_;

    /// 监听的socket，以socket_id为key
    std::unordered_map<int32_t, KcpIO *> listeners_;

    int64_t next_accept_timeout_ = 0; // 下一次扫描accept超时的时间戳(ms)
};

#endif
