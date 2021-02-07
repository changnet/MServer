#pragma once

// 主要是实现屏幕、文件双向输出。一开始考虑使用重定向、tee来实现，但是在跨平台、日志文件截断
// 方面不太好用。nohub方式不停服无法截断日志文件，查看日志也不太方便

#include <cstdio>

#include "../config.hpp"
#include "../global/types.hpp"

// 日志输出类型
enum LogType
{
    LT_NONE    = 0,
    LT_FILE    = 1, // 输出到指定文件
    LT_LPRINTF = 2, // 用于lua实现异步PRINTF宏定义
    LT_LERROR  = 3, // lua错误日志
    LT_CPRINTF = 4, // C异步PRINTF宏定义
    LT_CERROR  = 5, // C异步错误日志

    LO_MAX
};

void set_app_name(const char *name);

/**
 * 设置日志参数
 * @dm: deamon，是否守护进程。为守护进程不会输出日志到stdcerr
 * @ppath: printf path,打印日志的文件路径
 * @epath: error path,错误日志的文件路径
 * @mpath: mongodb path,mongodb日志路径
 */
void set_log_args(bool dm, const char *ppath, const char *epath,
                  const char *mpath);

void write_prefix(FILE *stream, const char *prefix, int64_t time);

/// 当前是否以后台模式运行
bool is_deamon();

/// 获取错误日志路径
const char *get_error_path();
/// 获取打印日志路径
const char *get_printf_path();

// /////////////////////////////////////////////////////////////////////////////
// 以下函数不直接调用，由宏定义调用
void __sync_log(const char *path, FILE *stream, const char *prefix,
                const char *fmt, ...);
void __async_log(const char *path, LogType type, const char *fmt, ...);
// /////////////////////////////////////////////////////////////////////////////

#ifdef _PRINTF_
    #define PRINTF(...) __async_log(get_printf_path(), LT_CPRINTF, __VA_ARGS__)
    // 线程安全，并且不需要依赖lev的时间
    #define PRINTF_R(...) \
        __sync_log(get_printf_path(), stdout, "CP", __VA_ARGS__)
#else
    #define PRINTF(...)
    #define PRINTF_R(...)
#endif

#ifdef _ERROR_
    #define ERROR(...) __async_log(get_error_path(), LT_CERROR, __VA_ARGS__)
    // 线程安全，并且不需要依赖lev的时间
    #define ERROR_R(...) __sync_log(get_error_path(), stderr, "CE", __VA_ARGS__)
#else
    #define ERROR(...)
    #define ERROR_R(...)
#endif
