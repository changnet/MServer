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

    // ---- IO 接口 ----
    /// 客户端形态：从自己的fd收包（服务端对端由 KcpAcceptorIO 喂，不走这里）
    int32_t recv(EVIO *w) override;
    /// 从 send_ 取帧 → ikcp_send + ikcp_flush
    int32_t send(EVIO *w) override;

    /// 监听：kcp收到了就可以直接读，不需要accept
    int32_t prepare_accept() override { return EV_READ; }
    /// 客户端：connect后直接开始收包（和 UdpIO 一样）
    int32_t prepare_connect() override { return EV_READ; }

    // ---- backend 专用 ----
    /// 建会话（KcpMgr::on_add 调用）
    bool create_kcp(uint32_t conv, int32_t listen_id, int32_t listen_fd,
                    const UdpAddr &addr);
    /// ★ 唯一的 ikcp_release 出口，且幂等
    void release_kcp();
    /// 把一段kcp报文喂进ikcp；has_data表示产出了完整业务包并已写入recv_
    int32_t input(const char *data, int32_t len, bool &has_data);
    /// 把ikcp里重组好的逻辑包搬进recv_，返回是否有产出
    bool drain();

    struct IKCPCB *kcp() const { return kcp_; }

    // ---- 主线程（start_kcp 之前预置）----
    void set_conv(uint32_t conv) { conv_ = conv; }

    int32_t listen_id() const { return listen_id_; }
    int32_t listen_fd() const { return listen_fd_; }
    const UdpAddr &peer() const { return peer_; }
    void touch(int64_t now) { last_active_ = now; }
    int64_t last_active() const { return last_active_; }

private:
    static int32_t output(const char *buf, int32_t len, struct IKCPCB *kcp,
                          void *user);

    /// 一次EV_READ最多读多少个datagram，防止一个疯狂发包的对端占死backend
    static constexpr int32_t MAX_RECV_PER_EVENT = 64;

    struct IKCPCB *kcp_ = nullptr;

    uint32_t conv_      = 0;
    UdpAddr peer_;      // ★ 地址只记在这里
    int32_t listen_id_  = 0; // 0 = 客户端形态
    int32_t listen_fd_  = netcompat::INVALID; // 客户端=自己的fd；服务端对端=监听fd
    int64_t last_active_ = 0;

    int64_t stat_drop_snd_ = 0; // ikcp_waitsnd 超限丢弃的条数
};

#endif
