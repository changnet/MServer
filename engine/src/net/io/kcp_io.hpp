#pragma once

#include "global/global.hpp"

#if defined(ENABLE_KCP)

#include "io.hpp"
#include "net/udp_addr.hpp"

#include "ikcp.h"

/* kcp的io读写（每个对端一个，恰好1个ikcpcb）
 *
 * 缓冲区中的帧格式为 [u32 size][payload]，size含自身。
 * 与udp不同，帧里不带地址：本连接的对端地址只记在 peer_ 上（见设计 §2.2）。
 *
 * 三种形态：
 *   客户端     自己的真实fd（已connect）+ 1个ikcpcb，收发走 recv/output
 *   服务端对端 fd = -1（虚拟连接）+ 1个ikcpcb，收由 KcpAcceptorIO 喂、发走 output
 *   服务端监听 由 KcpAcceptorIO 承担，本类不参与
 */
class KcpIO final : public IO
{
public:
    ~KcpIO();
    explicit KcpIO();

    /// 客户端形态：从自己的fd收包（服务端对端由 KcpAcceptorIO 喂，不走这里）
    int32_t recv(EVIO *w) override;
    /// 从 send_ 取帧 → ikcp_send + ikcp_flush
    int32_t send(EVIO *w) override;


    int32_t prepare_accept() override { return EV_ERROR; }
    int32_t prepare_connect() override { return EV_READ; }

    // socket初始化完成，开始初始化读写事件时调用，在业务线程执行
    virtual bool init_event(EVIO *w, lua_State *L, int32_t index);
    // socket关闭，通知backend线程移除事件，在业务线程执行
    virtual bool uninit_event(EVIO *w, lua_State *L, int32_t index);

    /**
     * 定时调用ikcp_update
     * @return 下次执行的时间戳
     */
    int64_t update(int64_t now);

    // ---- backend 专用 ----
    /// 建会话（KcpMgr::on_add 调用）
    bool create_kcp(uint32_t conv, int32_t listen_id, int32_t listen_fd,
                    const UdpAddr &addr);
    /// ★ 唯一的 ikcp_release 出口，且幂等
    void release_kcp();
    /**
     * 把一段数据喂给kcp，输出有序的业务数据
     * @return <0=出错 0=kcp控制报文 >0=数据长度
     */
    int32_t input(const char *data, int32_t len);

    // 设置kcp的会话id
    void set_conv(uint32_t conv) { conv_ = conv; }

    int32_t listen_id() const { return listen_id_; }
    int32_t listen_fd() const { return listen_fd_; }
    const UdpAddr &peer() const { return peer_; }

private:
    static int32_t output(const char *buf, int32_t len, struct IKCPCB *kcp,
                          void *user);

    /// 一次EV_READ最多读多少个datagram，防止一个疯狂发包的对端占死backend
    static constexpr int32_t MAX_RECV_PER_EVENT = 64;

    struct IKCPCB *kcp_ = nullptr;

    uint32_t conv_      = 0; // kcp的会话id（conversation）
    UdpAddr peer_;      // ★ 地址只记在这里
    int32_t listen_id_  = 0; // 0 = 客户端形态
    int32_t listen_fd_  = netcompat::INVALID; // 客户端=自己的fd；服务端对端=监听fd
};

#endif
