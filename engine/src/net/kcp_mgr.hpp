#pragma once

#include <unordered_map>
#include <vector>

#include "global/global.hpp"

#include "net/udp_addr.hpp"
#include "thread/thread_context.hpp"

#if defined(ENABLE_KCP)

class EVIO;
class KcpAcceptorIO;

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
 * kcp 会话管理器（io线程独占，单例挂在 EVBackend 上）
 *
 * 只保留"身份表"：
 *   conns_     socket_id → 连接 EVIO（身份一律用 socket_id）
 *
 * 路由（listen_id + 源地址 → socket_id）和"接入窗口缓存"都已经下沉到
 * 各自的 KcpAcceptorIO 上 —— 因为一个 kcp 监听 fd 上承载 N 个对端，只有
 * 监听socket自己才知道这些对端属于它，放在全局扁平表里会让两个监听socket
 * 上的同一个客户端地址互相覆盖。
 *
 * 定时不用 multimap：所有会话的 interval 都是同一个 KCP_INTERVAL，
 * 所以只保留一个全局节拍 next_update_ + 游标分片，见设计 §4。
 */
class KcpMgr
{
public:
    /// io线程join之后由 ~EVBackend 触发，释放所有残留会话
    ~KcpMgr();

    // ---- 来自 worker 的消息（在io线程执行）----
    void on_add(ThreadMessage *m); // KCP_ADD
    void on_del(ThreadMessage *m); // KCP_DEL（含"业务拒绝接入"）

    // ---- 监听socket登记（由 KcpAcceptorIO 在io线程调用）----
    void reg_acceptor(int32_t listen_id, KcpAcceptorIO *acc);
    void unreg_acceptor(int32_t listen_id);

    // ---- 生命周期 ----
    /// 唯一的回收点：释放 ikcpcb + 摘 conns_/已建立表 + （虚拟连接）解io线程引用
    void remove(int32_t conn_id, bool notify_worker);

    // ---- 定时（见设计 §4）----
    /// 到点就 tick（并顺带驱动 accept 表超时回收），返回下次唤醒建议的ms，-1 表示不需要被唤醒
    int64_t update(int64_t now);
    /// 有数据往来时调用（仅用于统计/调试）
    void touch(int32_t conn_id);

private:
    /// 一次节拍：游标分片遍历 conns_
    /// @return true 表示这一拍没处理完，剩下的要立刻继续
    bool tick(int64_t now);

    /// KCP_ADD 失败/被拒时，把accept表里对应的项删掉
    void drop_accepting(const KcpAddMsg *msg);

    /// 身份表：socket_id → 连接 EVIO（★ 身份一律用 socket_id）
    std::unordered_map<int32_t, EVIO *> conns_;
    /// 身份表的摊平迭代视图，分片用
    std::vector<int32_t> conn_ids_;
    size_t cursor_ = 0;

    /// listen_id → 监听socket的acceptor（它的表里放着这个监听fd上的所有对端）
    std::unordered_map<int32_t, KcpAcceptorIO *> acceptors_;

    int64_t next_update_ = 0; // 绝对时间(ms)，0 = 无会话
};

#endif
