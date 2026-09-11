#pragma once

#include "global/global.hpp"

#if defined(ENABLE_KCP)

#include "io.hpp"

/* kcp的监听socket（唯一一个，只路由，0个ikcpcb）
 *
 * 内核把包递给监听fd时只给一个 sockaddr，不会告诉我们是哪个 socket_id，
 * 所以这里负责 recvfrom + 查反向索引 route_ + 把包喂给对应连接的 ikcp。
 * 未接入的对端由 KcpMgr::park 缓存首包并通知 worker 决定是否接入。
 */
class KcpAcceptorIO final : public IO
{
public:
    ~KcpAcceptorIO();
    explicit KcpAcceptorIO();

    /// recvfrom + 路由 + park
    int32_t recv(EVIO *w) override;
    /// 监听socket从不发数据
    int32_t send(EVIO *w) override { return EV_NONE; }

    int32_t prepare_accept() override { return EV_READ; }
    int32_t prepare_connect() override { return EV_READ; }

private:
    /// 一次EV_READ最多读多少个datagram
    static constexpr int32_t MAX_RECV_PER_EVENT = 128;
};

#endif
