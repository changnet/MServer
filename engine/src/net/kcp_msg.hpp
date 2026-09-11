#pragma once

#include "global/global.hpp"

#include "net/udp_addr.hpp"

class EVIO;

/**
 * kcp 的线程间消息载荷（全部POD，按值塞进 ThreadMessage::buffer()）
 *
 * 控制面（建/删会话）走 ThreadContext 消息队列，数据面走 Buffer + EV_WRITE/EV_READ，
 * 详见 project/kcp_design.md §1.4。跨线程传 EVIO* 是框架既有做法（add_watcher_event
 * 就是这么干的），安全性靠 EVIO::M_REF_* 引用计数保证。
 */

#pragma pack(push, 1)

/// worker → backend：建一条 kcp 会话（服务端对端 / 客户端形态共用）
struct KcpAddMsg
{
    EVIO    *conn_w;    // 这条连接的 EVIO（主线程已建好，M_REF_BACKEND 已置位）
    int32_t  listen_id; // 服务端对端：监听 socket_id；客户端形态：0
    int32_t  listen_fd; // sendto 用的 fd（服务端对端=监听 fd；客户端=自己的 fd）
    uint32_t conv;      // 会话号（服务端由首包解码；客户端自己随机）
    UdpAddr  addr;      // 对端地址（客户端形态保持默认值 AF_UNSPEC）
};

/// worker → backend：删除一条 kcp 会话
struct KcpDelMsg
{
    int32_t conn_id; // 这条连接的 socket_id
};

/// backend → worker：发现新客户端
struct KcpAcceptMsg
{
    int32_t listen_id;
    uint32_t conv;
    UdpAddr addr;
};

#pragma pack(pop)

static_assert(offsetof(KcpAcceptMsg, conv) == 4, "KcpAcceptMsg layout changed");
static_assert(offsetof(KcpAcceptMsg, addr) == 8, "KcpAcceptMsg layout changed");
static_assert(sizeof(KcpAcceptMsg) == 28, "KcpAcceptMsg must be 28 bytes");
