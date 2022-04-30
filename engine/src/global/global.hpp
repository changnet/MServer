#pragma once

#include "platform.hpp"

#include <cerrno>      /* errno */
#include <cstdarg>     /* va_start va_end ... */
#include <cstdio>      /* c compatible,like printf */
#include <cstdlib>     /* malloc */
#include <cstring>     /* memset memcpy */
#include <ctime>       /* time function */
#include <limits.h>    /* PATH_MAX */
#include <sys/types.h> /* linux sys types like pid_t */

// 常用的头文件，避免需要手动include
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>

#include "../config.hpp" /* config paramter */

#include "types.hpp" /* base data type */

// those functions are easily make mistake,not allow to use project"
#include "banned.hpp"

#include "dbg_mem.hpp"

// 底层C日志相关函数，脚本最终也在调用C来打印日志
#include "../log/log.hpp"

/**
 * 格式化std::string，需要C++20才有std::format，这个暂时用一下
 */
static_assert(__cplusplus < 202002L);
extern std::string std_format(const char *fmt, ...);
#define STD_FMT(...) std_format(__VA_ARGS__)

// 把常量值转成字符串
// https://gcc.gnu.org/onlinedocs/gcc-9.2.0/cpp/Stringizing.html
/**
 * 把变量s转换成常量字符串
 */
#define STR(s) #s

/**
 * 把常量、宏转换为常量字符串
 */
#define XSTR(s) STR(s)

/* Qt style unused paramter handler */
#define UNUSED(x) (void)x

/* 分支预测，汇编优化。逻辑上不会改变cond的值,C++ 20之后，请使用stl中的likely、unlikely */
static_assert(__cplusplus < 202002L);
#ifdef __windows__
    #define EXPECT_FALSE(cond) cond
    #define EXPECT_TRUE(cond)  cond
#else
    #define EXPECT_FALSE(cond) __builtin_expect(!!(cond), 0)
    #define EXPECT_TRUE(cond)  __builtin_expect(!!(cond), 1)
#endif

/* terminated without destroying any object and without calling any of the
 * functions passed to atexit or at_quick_exit
 */
#define FATAL(...)           \
    do                       \
    {                        \
        ELOG_R(__VA_ARGS__); \
        ::abort();           \
    } while (0)

/// 求C数组的数量
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
