#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "global/global.hpp"

#include "net/udp_addr.hpp"
#include "thread/thread_context.hpp"

#if defined(ENABLE_KCP)

class EVIO;

/**
 * UdpAddr 的hash函数（UdpAddr 已有 operator==，直接 memcmp 20 字节）
 */
struct UdpAddrHash
{
    size_t operator()(const UdpAddr &a) const
    {
        const uint8_t *p = reinterpret_cast<const uint8_t *>(&a);
        size_t h         = 1469598103934665603ull; // FNV-1a
        for (size_t i = 0; i < sizeof(UdpAddr); ++i)
        {
            h ^= p[i];
            h *= 1099511628211ull;
        }
        return h;
    }
};

/**
 * kcp 会话管理器（backend 线程独占，单例挂在 EVBackend 上）
 *
 * 三张表：
 *   conns_     身份表   socket_id → 连接 EVIO      （身份一律用 socket_id）
 *   route_     反向索引 listen_id + 源地址 → socket_id（UDP版的fd_mgr_，见设计 §2.5）
 *   parked_    接入窗口缓存（首包 → KCP_ADD 期间攒包，零丢包回放）
 *
 * 定时不用 multimap：所有会话的 interval 都是同一个 KCP_INTERVAL，
 * 所以只保留一个全局节拍 next_update_ + 游标分片，见设计 §4。
 */
class KcpMgr
{
public:
    /// backend线程join之后由 ~EVBackend 触发，释放所有残留会话
    ~KcpMgr();

    // ---- 来自 worker 的消息（在 backend 线程执行）----
    void on_add(ThreadMessage *m); // KCP_ADD
    void on_del(ThreadMessage *m); // KCP_DEL

    // ---- 服务端收包路径（KcpAcceptorIO 调用）----
    /// (监听id, 源地址) → 连接 EVIO*；未接入返回 nullptr
    EVIO *route(int32_t listen_id, const UdpAddr &addr) const;
    /// 接入窗口内缓存首包（不丢包）
    bool park(int32_t listen_id, int32_t listen_fd, int32_t worker_addr,
              const UdpAddr &addr, uint32_t conv, const char *data, int32_t len);
    /// 接入前又来包，追加到已 park 的数据后面
    bool append_parked(const UdpAddr &addr, const char *data, int32_t len);

    // ---- 生命周期 ----
    /// 唯一的回收点：释放 ikcpcb + 摘 conns_/route_ + （虚拟连接）解 backend 引用
    void remove(int32_t conn_id, bool notify_worker);

    // ---- 定时（见设计 §4）----
    /// 到点就 tick，返回"下次唤醒建议的ms"，-1 表示无会话不需要被唤醒
    int64_t update(int64_t now);
    /// 有数据往来时调用（仅用于统计/调试）
    void touch(int32_t conn_id);

    // ---- 统计 ----
    void count_drop_unknown() { ++stat_drop_unknown_; }

private:
    /// 一次节拍：游标分片遍历 conns_
    /// @return true 表示这一拍没处理完，剩下的要立刻继续
    bool tick(int64_t now);

    /// 身份表：socket_id → 连接 EVIO（★ 身份一律用 socket_id）
    std::unordered_map<int32_t, EVIO *> conns_;
    /// 反向索引（UDP版fd_mgr_）：listen_id + 源地址 → socket_id
    std::unordered_map<int32_t,
                       std::unordered_map<UdpAddr, int32_t, UdpAddrHash>>
        route_;
    /// 反向索引的摊平迭代视图，分片用
    std::vector<int32_t> conn_ids_;
    size_t cursor_ = 0;

    /// 接入窗口期缓存（按 addr 索引，全进程唯一）
    struct Parked
    {
        int32_t listen_id;
        int32_t listen_fd;
        uint32_t conv;
        std::string data; // 按值存，无裸指针、无手写free
    };
    std::unordered_map<UdpAddr, Parked, UdpAddrHash> parked_;

    int64_t next_update_       = 0; // 绝对时间(ms)，0 = 无会话
    int64_t stat_drop_unknown_ = 0; // 非PUSH包 / parked_ 满
    int64_t stat_drop_overflow_ = 0; // 溢出丢弃计数（预留给上层统计）
};

#endif
