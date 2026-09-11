#pragma once

// 本文件会被 global/global.hpp 和 log/log.hpp 直接include，
// 而 int32_t/uint8_t 由 global/types.hpp 提供、在 global.hpp 里排在 config.hpp 之后，
// 所以这里必须自己带上，保证单独include也能编译
#include <cstdint>
#include <cstddef>

// 这个不要在这里定义，而是直接使用release build
// #define NDEBUG

// #define NMEM_DEBUG
#define NDBG_MEM_TRACE

// 是否启用ssl调试日志
// #define SSL_DBG

// ===================== KCP =====================
// 注：ENABLE_KCP 由 CMake 的 target_compile_definitions 定义（默认开启），
//     这里不重复定义，只在 CMake 里控制开关

// 单个 KCP 报文的最大长度（含 ikcp 头）。1400 = 以太网 MTU 1500 - IP 20 - UDP 8 - 余量
static constexpr int32_t KCP_MTU = 1400;

// ikcp_nodelay(kcp, nodelay, interval, resend, nc)
//   nodelay =0 : 不启用 nodelay 模式
//   interval=40: 内部更新时钟间隔 40ms
//   resend  =0 : 不启用快速重传
//   nc      =0 : 启用拥塞控制
static constexpr int32_t KCP_NODELAY  = 0;
static constexpr int32_t KCP_INTERVAL = 40;
static constexpr int32_t KCP_RESEND   = 0;
static constexpr int32_t KCP_NC       = 0;

// ikcp_wndsize(kcp, sndwnd, rcvwnd)
// rcvwnd 同时决定接收端单个逻辑包的重组上限（约 rcvwnd * mss），是防超大包的第一道墙
static constexpr int32_t KCP_SND_WND = 128;
static constexpr int32_t KCP_RCV_WND = 128;

// 单个逻辑包上限。ikcp_send 会按 mss 自动分片，frg 字段是 1 字节，
// 理论上限 = 255 * (KCP_MTU - 24)；这里取 64KB，超过则 Lua 侧报错
static constexpr int32_t KCP_MAX_MSG = 64 * 1024;

// ikcp_waitsnd 上限。超过就丢这一条 + 计数，不 sleep、不阻塞整条 fd
static constexpr int32_t KCP_MAX_WAIT_SND = 256;

// 同时存在的 kcp 会话上限（客户端形态 + 服务端对端，全进程）
static constexpr int32_t KCP_MAX_SESSION = 4096;

// 接入窗口（首包 → KCP_ADD）期间 park 的地址数上限
static constexpr int32_t KCP_MAX_PARKED = 1024;

// 接入窗口内单个地址累计 park 的字节上限。
// 只有 IKCP_CMD_PUSH 才会 park，但接入窗口内同一个 addr 可以持续发包，
// 这里必须封顶，否则一次 addr 洪水就能把 parked_ 的 std::string 撑爆
static constexpr int32_t KCP_MAX_PARKED_DATA = 64 * 1024;

// 一次节拍最多 update 多少个会话（游标分片，见 project/kcp_design.md §4.2）
static constexpr int32_t KCP_TICK_BATCH = 512;

// 【坑】以下两个常量定义在 ikcp.c 里，ikcp.h 没有导出，必须自己抄一份
static constexpr uint8_t IKCP_CMD_PUSH = 81;
static constexpr int32_t IKCP_OVERHEAD = 24;
