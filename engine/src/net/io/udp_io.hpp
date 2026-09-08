#pragma once

#include "global/global.hpp"

#include "io.hpp"
#include "net/udp_addr.hpp"

/* udp的io读写
 *
 * 缓冲区中的帧格式为 [u32 size][UdpAddr 20B][payload]，size含自身。
 * 地址只用于本机的sendto，不会被发送出去，因此不需要考虑字节序。
 */
class UdpIO final : public IO
{
public:
    ~UdpIO();
    explicit UdpIO();

    /**
     * 接收数据（此函数在io线程执行）
     * 一个EV_READ可能对应多个datagram，这里循环读取直到EAGAIN
     * @return int32_t
     */
    int32_t recv(EVIO *w) override;
    /**
     * 发送数据（此函数在io线程执行）
     * @return int32_t
     */
    int32_t send(EVIO *w) override;

private:
    /// 一次EV_READ最多读取多少个datagram，防止一个疯狂发包的对端占死backend线程
    static constexpr int32_t MAX_RECV_PER_EVENT = 128;
};
