#pragma once

// 主要是实现屏幕、文件双向输出。一开始考虑使用重定向、tee来实现，但是在跨平台、日志文件截断
// 方面不太好用。nohub方式不停服无法截断日志文件，查看日志也不太方便

#include <cstdio>
#include <mutex>

#include "config.hpp"
#include "global/types.hpp"

namespace log_util
{
    /**
     * @brief 日志前缀，必须保证即使通过std::vector管理发生复制时，
     * 指向的string内存也必须不变，所以std::vector只能存指针
     * 
     * std::string不被修改的话，其返回的c_str()是一直有效的
     */
    class Prefix
    {
    public:
        Prefix()
        {
        }
        ~Prefix()
        {
            for (auto& v : v_)
            {
                delete v;
            }
        }
        const char* address(const char* name)
        {
            std::scoped_lock<std::mutex> sl(mutex_);
            std::string n(name);
            for (auto &s : v_)
            {
                if (n == *s) return s->c_str();
            }

            v_.push_back(new std::string(name));
            return v_.back()->c_str();
        }
    private:
        std::mutex mutex_;
        std::vector<std::string *> v_;
    };
// 日志输出类型
enum LogType
{
    LT_NONE    = 0,
    LT_LOGFILE = 1, // 写入日志到指定文件
    LT_LPRINTF = 2, // 用于lua实现异步PRINTF宏定义
    LT_LERROR  = 3, // lua错误日志
    LT_CPRINTF = 4, // C异步PRINTF宏定义
    LT_CERROR  = 5, // C异步错误日志
    LT_FILE    = 6, // 写入内容到文件，但不包含时间的日志前缀，不会自动换行

    LO_MAX
};

void set_prefix_name(const char *name);
const char *get_prefix_name();

// __FILE__显示的是文件被编译时的路径，在cmake下是全路径
// 这时可以通过cmake计算出根目录，丢掉根目录的路径即变成相对路径(relative file)
#ifndef SRC_PATH_SIZE
    #define SRC_PATH_SIZE 0
#endif
#define __RL_FILE__ (__FILE__ + SRC_PATH_SIZE)

/**
 * 设置日志参数
 * @deamon: deamon，是否守护进程。为守护进程不会输出日志到stdcerr
 * @ppath: printf path,打印日志的文件路径
 * @epath: error path,错误日志的文件路径
 */
void set_log_args(bool deamon, const char *ppath, const char *epath);

size_t write_prefix(FILE *stream, const char *prefix, const char *type, int64_t time);

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
} // namespace log_util

/**
 * print log，线程不安全，需要日志线程初始化后才能调用
 * 非主线程调用此函数，日志时间戳可能不会被更新
 */
#define PLOG(...) \
    log_util::__async_log(log_util::get_printf_path(), log_util::LT_CPRINTF, __VA_ARGS__)

/**
 * 线程安全日志，不依赖日志线程，不依赖主线程的时间
 */
#define PLOG_R(...) \
    log_util::__sync_log(log_util::get_printf_path(), stdout, "CP", __VA_ARGS__)

// TODO ## __VA_ARGS__ 中的##在ELOG("test")
// 这种只有一个参数的情况下去掉前面的逗号，但这不是标准的用法。
// C++ 20之后 ## __VA_ARGS__ 改成 __VA_OPT__(,) __VA_ARGS__

/**
 * error log，线程不安全，需要日志线程初始化后才能调用
 * 非主线程调用此函数，日志时间戳可能不会被更新
 */
#define ELOG(fmt, ...)                       \
    log_util::__async_log(log_util::get_error_path(), log_util::LT_CERROR, \
                    __FILE__ ":" XSTR(__LINE__) " " fmt, ##__VA_ARGS__)

/**
 * 线程安全日志，不依赖日志线程，不依赖主线程的时间
 */
#define ELOG_R(fmt, ...)                       \
    log_util::__sync_log(log_util::get_error_path(), stderr, "CE", \
                __FILE__ ":" XSTR(__LINE__) " " fmt, ##__VA_ARGS__)

/**
 * 严重错误，打印错误信息并且终止程序
 */
#define FATAL(...)           \
    do                       \
    {                        \
        ELOG_R(__VA_ARGS__); \
        ::abort();           \
    } while (0)