#pragma once

#include "global/global.hpp"

#if defined(ENABLE_KCP)

#include <mutex>
#include <string>
#include <unordered_map>

#include "io.hpp"

#include "net/kcp_mgr.hpp" // UdpAddrHash
#include "net/udp_addr.hpp"

class EVIO;

/* kcp的监听socket（只收包 + 路由，0个ikcpcb）
 *
 * 一个kcp监听fd上承载N个对端，而内核递包时只给sockaddr、不会给socket_id，
 * 所以这里负责：
 *   ① recvfrom 收包
 *   ② 用 UdpAddr 查"已建立连接表" → 命中就直接喂那条连接的 ikcp
 *   ③ 未命中则放进"accept表"缓存首包，并返回EV_ACCEPT派发到业务线程，
 *      由业务线程走和 tcp 完全一样的 on_accepting 流程决定是否接入
 *   ④ 业务线程发来 KCP_ADD 后，由 KcpMgr::on_add 调 promote 把这条连接
 *      从accept表晋升到"已建立表"，并按序回放接入窗口内缓存的数据（零丢包）
 *   ⑤ 业务拒绝接入时发 KCP_DEL，drop_accepting 直接删掉该项
 *   ⑥ 16s 还没晋升的对端由 sweep 回收
 *
 * ★ 与 tcp 的关键差异：tcp 的 AcceptBuffer 里放的是已经完成内核握手的连接
 *   （fd 自带生命周期），kcp 拿到的是"还没有连接的若干字节"，所以额外需要
 *   "已交给业务线程"状态和超时回收；而 EV_ACCEPT 的派发与业务侧 do_accept
 *   循环可以完全复用。
 *
 * 表的归属：
 *   accepting_    io线程写、业务线程读（pop_accept）→ 需要锁
 *   established_  仅io线程访问（promote / on_dgram / unestablish）→ 无锁
 */
class KcpAcceptorIO final : public IO
{
public:
    ~KcpAcceptorIO();
    explicit KcpAcceptorIO();

    // ---- IO 接口 ----
    /**
     * 监听socket注册的是EV_ACCEPT，内核可读走 accept() 分支，
     * 这里不会被调用；保留实现只是为了满足IO的纯虚接口
     */
    int32_t recv(EVIO *w) override;
    /// 监听socket从不发数据
    int32_t send(EVIO *w) override { return EV_NONE; }

    /// ★ 与 TcpIO 一致：监听socket注册成 EV_ACCEPT
    int32_t prepare_accept() override { return EV_ACCEPT; }
    int32_t prepare_connect() override { return EV_READ; }

    /**
     * 监听fd可读：recvfrom + 路由（与 TcpIO::accept 同构，跑在io线程）
     * @return EV_ACCEPT 本轮产生了新对端，需要派发EV_ACCEPT给业务线程；
     *         EV_NONE   只是已有对端的数据包，不唤醒业务线程
     */
    int32_t accept(EVIO *w) override;

    // ---- accept表：业务线程侧 ----
    /**
     * 取出一个待接入的对端（业务线程收到EV_ACCEPT后循环调用）
     * @param addr 出参，对端地址
     * @param conv 出参，会话号
     * @return true 表示成功取得一份待接入的对端
     */
    bool pop_accept(UdpAddr &addr, uint32_t &conv);

    // ---- 已建立表 / 晋升：io线程侧（由 KcpMgr 调用）----
    /**
     * 把一个对端从accept表晋升到已建立表，并取走接入前缓存的数据
     * ★ 必须在 create_kcp 之后调用，且"摘accept表 + 挂已建立表"要在同一次
     *   临界区内完成，否则中间到达的包会路由到连接但 io->kcp() 还是 nullptr
     */
    void promote(const UdpAddr &addr, EVIO *w, std::string &out);
    /// 摘除已建立的映射（KcpMgr::remove 调用）
    void unestablish(const UdpAddr &addr);
    /// 业务拒绝接入：直接删掉accept表项（对应tcp的 close(fd)）
    void drop_accepting(const UdpAddr &addr);
    /// 回收 16s 内没晋升的对端（KcpMgr 每5秒调一次）
    void sweep(int64_t now);

private:
    /// 一个还没晋升的对端
    struct AcceptEntry
    {
        uint32_t conv     = 0;
        int64_t create_ms = 0; // 用于16s回收
        bool notified     = false; // 是否已交给业务线程，防止同一对端被accept两次
        std::string data;          // 接入前缓存的数据，KCP_MAX_PARKED_DATA封顶
    };

    /**
     * 处理一个数据报（io线程）
     * @return true 表示产生了新的待接入对端（需要派发EV_ACCEPT唤醒业务线程）
     */
    bool on_dgram(const UdpAddr &addr, const char *data, int32_t len);
    /// 洪水时不能刷爆日志，同一个原因每N毫秒最多一条
    void log_limited(int64_t now, const char *what, const UdpAddr &addr,
                     int32_t extra);

    /// 一次EV_ACCEPT最多读多少个datagram，防止一个疯狂发包的对端占死io线程
    static constexpr int32_t MAX_RECV_PER_EVENT = 128;

    /// accept表：io线程写、业务线程读
    std::mutex accept_mutex_;
    std::unordered_map<UdpAddr, AcceptEntry, UdpAddrHash> accepting_;

    /// 已建立表：仅io线程访问，无需锁
    std::unordered_map<UdpAddr, EVIO *, UdpAddrHash> established_;

    int64_t last_log_ms_ = 0;
};

#endif
