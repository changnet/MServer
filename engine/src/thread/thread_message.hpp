#pragma once

#include "global/global.hpp"

#if defined(ENABLE_KCP)
    /**
     * KcpAddMsg 按值含 UdpAddr，而 udp_addr.hpp 会拉进 winsock2.h/ws2tcpip.h。
     * 用 #if 圈起来，让这个头依赖只存在于 ENABLE_KCP=ON 的构建里
     * （socket.hpp 特意只写 `struct UdpAddr;` 前置声明就是为了躲它）
     */
    #include "net/udp_addr.hpp"
#endif

class EVIO;

/// backend(io)线程的投递地址。C++ 与 lua 侧(server/src/engine/startup.lua)必须保持一致
static constexpr int32_t BACKEND_ADDR = -1;

// 线程数据交互结构
struct ThreadMessage final
{
    enum
    {
        // ---- 0 ~ 63：C++ 预留 ----
        NONE   = 0, // 无作用，通常只是唤醒线程
        TIMER  = 1, // 定时器
        SIGNAL = 2, // 信号
        SOCKET = 3, // 网络消息
        // 4 曾用于 KCP_ACCEPT（backend → worker：发现新客户端，请决定是否接入）。
        // kcp 的接入现在完全复用 tcp 的 EV_ACCEPT 派发路径（见 KcpAcceptorIO），
        // 不再需要这条消息，编号 4 作废且不再复用
        KCP_ADD = 5, // worker → backend：建会话（建 ikcpcb + 登记）
        KCP_DEL = 6, // worker → backend：删会话（释放 ikcpcb + 摘路由）
        // ---- 64 ~ 127：Lua 预留，C++ 不解释 ----
        LUA_BASE = 64,
    };

    ThreadMessage(int32_t src, int32_t dst, uint16_t type,
                  int32_t usize)
    {
        mask_  = 0;
        src_   = src;
        dst_   = dst;
        type_  = type;
        usize_ = usize;
    }
    ~ThreadMessage()
    {
    }

    // 获取缓冲区指针
    char *buffer() noexcept
    {
        // C++ 不支持Flexible Array Member，直接强转.C++ 20可用std::span
        return reinterpret_cast<char *>(this + 1);
    }

    uint16_t mask_; // 掩码标记 0位从内存分配
    uint16_t type_; // 消息类型
    int32_t src_; // 来源地址
    int32_t dst_; // 目标地址
    int32_t usize_; // 自定义数据长度
    // 这个结构是flexible array，后面还有自定义数据
};

#if defined(ENABLE_KCP)

/**
 * kcp 的线程间消息载荷（全部POD，按值塞进 ThreadMessage::buffer()）
 *
 * 控制面（建/删会话）走 ThreadContext 消息队列，数据面走 Buffer + EV_WRITE/EV_READ。
 * 跨线程传 EVIO* 是框架既有做法（add_watcher_event 就是这么干的），
 * 安全性靠 EVIO::M_REF_* 引用计数保证。
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

/// worker → backend：删除一条 kcp 会话 / 删除一个 accept 表项
struct KcpDelMsg
{
    /**
     * 会话的 socket_id。
     * ★ 0 表示"业务拒绝接入"：删的是 accept 表里还没晋升的项，此时用 listen_id + addr
     */
    int32_t conn_id;
    int32_t listen_id; // conn_id == 0 时使用：哪个监听 socket
    UdpAddr addr;      // conn_id == 0 时使用：被拒绝的对端地址
};

#pragma pack(pop)

// EVIO* 在 64 位下是 8 字节，32 位下是 4 字节，所以只在 64 位下固化布局
static_assert(sizeof(void *) != 8 || offsetof(KcpAddMsg, addr) == 20,
              "KcpAddMsg layout changed");
static_assert(sizeof(void *) != 8 || sizeof(KcpAddMsg) == 40,
              "KcpAddMsg must be 40 bytes");
static_assert(offsetof(KcpDelMsg, addr) == 8, "KcpDelMsg layout changed");
static_assert(sizeof(KcpDelMsg) == 28, "KcpDelMsg must be 28 bytes");

#endif
