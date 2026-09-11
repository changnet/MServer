#pragma once

#include "net/net_compat.hpp"

/**
 * ICMP port unreachable：上一个sendto发送到了一个已关闭的对端
 * windows下是WSAECONNRESET，linux下是ECONNREFUSED。这不是错误，
 * 必须忽略，否则udp/kcp连接会被误判为出错并关闭
 *
 * 原本是 udp_io.cpp 里的static函数，kcp_io / kcp_acceptor_io 也要用，
 * 因此抽到这里共用
 */
static inline bool is_icmp_unreachable(int32_t e)
{
#ifdef __windows__
    return WSAECONNRESET == e || WSAENETRESET == e || WSAECONNABORTED == e;
#else
    return ECONNREFUSED == e || ECONNRESET == e;
#endif
}
