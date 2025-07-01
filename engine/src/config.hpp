#pragma once

// 这个不要在这里定义，而是直接使用release build
// #define NDEBUG

// #define NMEM_DEBUG
#define NDBG_MEM_TRACE

// 默认lua入口文件
constexpr const char *LUA_ENTERANCE = "../src/main.lua";

// 是否使用ipv4(默认使用ipv6双栈)
// #define IP_V4 "IPV4"

// 是否启用ssl调试日志
// #define SSL_DBG
