#pragma once

// 主要是实现屏幕、文件双向输出。一开始考虑使用重定向、tee来实现，但是在跨平台、日志文件截断
// 方面不太好用。nohub方式不停服无法截断日志文件，查看日志也不太方便

#include <cstdio>
#include <mutex>

#include "config.hpp"
#include "global/types.hpp"

namespace log_util
{
// __FILE__显示的是文件被编译时的路径，在cmake下是全路径
// 这时可以通过cmake计算出根目录，丢掉根目录的路径即变成相对路径(relative file)
#ifndef SRC_PATH_SIZE
    #define SRC_PATH_SIZE 0
#endif
#define __RL_FILE__ (__FILE__ + SRC_PATH_SIZE)

    // /////////////////////////////////////////////////////////////////////////////
// 以下函数不直接调用，由宏定义调用

void __sync_log(int32_t type, const char *str, int32_t len);
void __async_log(int32_t type, const char *str, int32_t len);
// /////////////////////////////////////////////////////////////////////////////

template<typename... Args>
const char *fmt_log_buffer(int32_t len, Args&&... args)
{
    thread_local char buffer[20480];
    len = snprintf(buffer, sizeof(buffer), std::forward<Args>(args)...);
    if (len >= (int32_t)sizeof(buffer)) len = (int32_t)sizeof(buffer) - 1;

    return buffer;
}

template<typename... Args> void sync_log(int32_t type, Args&&... args)
{
    int32_t len     = 0;
    const char *str = fmt_log_buffer(len, std::forward<Args>(args)...);
    __sync_log(type, str, len);
}

template <typename... Args>
void async_log(int32_t type, Args &&...args)
{
    int32_t len     = 0;
    const char *str = fmt_log_buffer(len, std::forward<Args>(args)...);
    __async_log(type, str, len);
}
} // namespace log_util

/**
 * @brief print log，线程不安全，需要日志线程初始化后才能调用
 * 非主线程调用此函数，日志时间戳可能不会被更新
 */
#define PLOG(...) log_util::async_log(1, __VA_ARGS__)

// TODO ## __VA_ARGS__ 中的##在ELOG("test")
// 这种只有一个参数的情况下去掉前面的逗号，但这不是标准的用法。
// C++ 20之后 ## __VA_ARGS__ 改成 __VA_OPT__(,) __VA_ARGS__

/**
 * @brief error log，线程不安全，需要日志线程初始化后才能调用
 * 非主线程调用此函数，日志时间戳可能不会被更新
 */
#define ELOG(fmt, ...)                                 \
    log_util::async_log(2, __FILE__ ":" XSTR(__LINE__) " " fmt, ##__VA_ARGS__)

/**
 * @brief 打印错误日志（仅在ELOG失效时应急使用）
 */
#define ELOG_R(fmt, ...) \
    log_util::sync_log(2, __FILE__ ":" XSTR(__LINE__) " " fmt, ##__VA_ARGS__)

/**
 * @brief 严重错误，打印错误信息并且终止程序
 */
#define FATAL(...)           \
    do                       \
    {                        \
        ELOG_R(__VA_ARGS__); \
        ::abort();           \
    } while (0)
