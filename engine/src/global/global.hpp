#pragma once

#include <cerrno>      /* errno */
#include <cstdarg>     /* va_start va_end ... */
#include <cstdio>      /* c compatible,like printf */
#include <cstdlib>     /* malloc */
#include <cstring>     /* memset memcpy */
#include <ctime>       /* time function */
#include <limits.h>    /* PATH_MAX */
#include <sys/types.h> /* linux sys types like pid_t */
#include <unistd.h>    /* POSIX api */

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
extern std::string std_format(const char *fmt, ...);
#define STD_FMT(...) std_format(__VA_ARGS__)

/* Qt style unused paramter handler */
#define UNUSED(x) (void)x

/* 分支预测，汇编优化。逻辑上不会改变cond的值 */
#define EXPECT_FALSE(cond) __builtin_expect(!!(cond), 0)
#define EXPECT_TRUE(cond)  __builtin_expect(!!(cond), 1)

/* terminated without destroying any object and without calling any of the
 * functions passed to atexit or at_quick_exit
 */
#define FATAL(...)          \
    do                      \
    {                       \
        ERROR(__VA_ARGS__); \
        ::abort();          \
    } while (0)

#define ARRAY_RESIZE(type, base, cur, cnt, init)            \
    if ((cnt) > (cur))                                      \
    {                                                       \
        uint32_t size = cur > 0 ? cur : 16;                 \
        while (size < (uint32_t)cnt)                        \
        {                                                   \
            size *= 2;                                      \
        }                                                   \
        type *tmp = new type[size];                         \
        init(tmp, sizeof(type) * size);                     \
        if (cur > 0) memcpy(tmp, base, sizeof(type) * cur); \
        delete[] base;                                      \
        base = tmp;                                         \
        cur  = size;                                        \
    }

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define ARRAY_NOINIT(base, size)
#define ARRAY_ZERO(base, size) memset((void *)(base), 0, size)
