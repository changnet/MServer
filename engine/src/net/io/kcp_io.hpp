#pragma once

#include "global/global.hpp"

#include "ikcp.h"
#include "io.hpp"
#include "net/udp_addr.hpp"

/* kcp的io读写，一个对象对应一种角色：
 *
 *   LISTENER   监听socket，持真实fd。N条对端复用它收发，路由表在 AcceptContext
 *   ACCEPTOR   服务端的一条对端（虚拟连接，fd = INVALID），收包由监听对象喂，发包借监听fd
 *   CONNECTOR  客户端，自己的真实fd，收发都走它
 *
 * 三种角色共用 IOT_KCP：io类型和lua的socket类是一对一的，不按io类型分派。
 * 缓冲区中的帧格式为 [u32 size][payload]，size含自身。
 */
class KcpIO final : public IO
{
public:
    ~KcpIO();
    explicit KcpIO();

    int32_t recv(EVIO *w) override;
    int32_t send(EVIO *w) override;
    int32_t set_option(lua_State *L) override;
    bool init_event(EVIO *w, lua_State *L, int32_t index) override;
    bool uninit_event(EVIO *w, lua_State *L, int32_t index) override;

    /**
     * 监听fd可读：recvfrom + 路由（io线程）
     * @return EV_ACCEPT 本轮出现了新对端，需要唤醒业务线程；
     *         EV_NONE   只是已有对端的数据包，不能唤醒业务线程
     */
    int32_t accept(EVIO *w) override;
    // 取出一个待接入的对端，返回它的vfd（业务线程）
    int64_t pop_accept(int32_t &e) override;
    void reject_accept(int64_t fd) override;

    // 建立kcp相关数据（backend线程）
    bool on_kcpmgr_add(EVIO *w);
    // 释放kcp相关数据（backend线程，最早且唯一的释放点，可重复调用）
    void on_kcpmgr_remove();

    // 把一条待接入的对端晋升为会话（backend线程，this是监听对象，w是那条对端）
    bool establish_accept(EVIO *w);
    // 从已建立表摘除（backend线程，this是监听对象）
    void remove_establish(EVIO *w);
    // 回收超时未接入的对端（backend线程）
    void remove_accept_timeout(int64_t now);
    // 关闭监听时，把名下的对端逐个交给 KcpMgr 回收（backend线程）
    void remove_establish_peers(bool flush);

    // 驱动定时器，返回距离下次需要执行的毫秒数
    int64_t update(int64_t now);
    // 把一段kcp数据喂给kcp，返回 0=控制报文 >0=业务数据长度 <0=非kcp数据
    int32_t input(const char *data, int32_t len);
    // 关闭前尽力把剩余数据交给kcp并flush一次（kcp没有优雅关闭，不保证送达）
    void flush(EVIO *w);

    const UdpAddr &peer() const
    {
        return peer_;
    }

    /**
     * 建立时一次性设置对端地址（backend线程，只在establish_accept里调用）
     *
     * peer_ 是路由表/已建立表的key，peer_sa_ 是发送用的现成结构。
     * 两者必须一起赋值：地址只在建连时转换一次，output热路径不再重复转换。
     * 分开赋值会出现"key对了但发到别处"这种静默错误，所以只留这一个入口。
     */
    void set_peer(const UdpAddr &addr)
    {
        peer_ = addr;
        addr.to_sockaddr(peer_sa_);
    }

    int64_t get_vfd() const
    {
        return vfd_;
    }

    // 作为acceptor时，获取所属监听socket的id
    int32_t get_main_id() const
    {
        return (int32_t)(vfd_ >> 32);
    }

private:
    // 未建立连接的对端
    struct AcceptEntry
    {
        int64_t vfd_      = 0; // 虚拟fd，用监听socket_id | 序号构建
        uint32_t conv     = 0; // kcp专用的会话id
        int64_t create_ms = 0; // 用于超时回收
        bool notified     = false; // 是否已交给业务线程，防止同一对端被accept两次
        UdpAddr addr;      // 对端地址，同时也是已建立表的key
        std::string data;  // 接入前缓存的数据，KCP_MAX_PARKED_DATA封顶
    };
    // 监听socket用来保存accept相关的上下文
    struct AcceptContext
    {
        int32_t fd_seed_   = 0; // 用于生成vfd的序号
        int32_t socket_id_ = 0; // 对应 EVIO::id_
        std::mutex mutex_;
        std::unordered_map<UdpAddr, AcceptEntry, UdpAddrHash> accepting_;

        /// 已建立表：仅io线程访问，无需锁
        std::unordered_map<UdpAddr, EVIO *, UdpAddrHash> established_;
    };

    // kcp库用于发送数据的接口
    static int32_t output(const char *buf, int32_t len, struct IKCPCB *kcp,
                          void *user);
    // 处理处于accept状态的数据，返回是否出现了新对端
    bool do_accept_data(const UdpAddr &addr, const char *data, int32_t len);
    // 构建唯一的vfd：高位是监听socket_id，低位是序号
    int64_t make_vfd();
    // 按ip输出错误日志
    void log_error(const char *what, const UdpAddr &addr, int32_t extra);

    /// 一次EV_READ最多读多少个datagram，防止一个疯狂发包的对端占死backend
    static constexpr int32_t MAX_RECV_PER_EVENT = 64;

    uint32_t conv_    = 0; // kcp的会话id（conversation）
    int32_t fd_       = 0; // output用的fd：CONNECTOR自己的，ACCEPTOR借监听的
    int64_t vfd_      = 0; // ACCEPTOR：accept时发给lua的虚拟fd，编码了所属监听socket
    UdpAddr peer_;         // ACCEPTOR：对端地址
    UdpSockAddr peer_sa_;  // ACCEPTOR：peer_ 预先转好的发送结构(32B)，热路径直接用

    struct IKCPCB *kcp_    = nullptr;
    AcceptContext *accept_ = nullptr; // 监听socket的accept上下文
};
