#pragma once

#include <cstdio>      /* c compatible,like printf */
#include <cstdlib>     /* c lib like malloc */
#include <iostream>    /* c++ base support */
#include <ctime>       /* time function */
#include <cstdarg>     /* va_start va_end ... */
#include <unistd.h>    /* POSIX api */
#include <sys/types.h> /* linux sys types like pid_t */
#include <cstring>     /* memset memcpy */
#include <cerrno>      /* errno */
#include <limits.h>    /* PATH_MAX */

#include "../config.h" /* config paramter */

/* assert with error msg,third parted code not include this file will work fine
   with old assert.
*/
#include "types.h"     /* base data type */
#include "assert.h"

// those functions are easily make mistake,not allow to use project"
#include "banned.h"

#include "dbg_mem.h"

////////////////////////////////////////////////////////////////////////////////
// 底层C日志相关函数，脚本最终也在调用C来打印日志

// 主要是实现屏幕、文件双向输出。一开始考虑使用重定向、tee来实现，但是在跨平台、日志文件截断
// 方面不太好用。nohub方式不停服无法截断日志文件，查看日志也不太方便

extern void set_app_name( const char *name );
extern void cerror_log ( const char *prefix,const char *fmt,... );
extern void cprintf_log( const char *prefix,const char *fmt,... );
extern void raw_cerror_log( time_t tm,const char *prefix,const char *fmt,... );
extern void raw_cprintf_log( time_t tm,const char *prefix,const char *fmt,... );

/* 设置日志参数
 * @dm: deamon，是否守护进程。为守护进程不会输出日志到stdcerr
 * @ppath: printf path,打印日志的文件路径
 * @epath: error path,错误日志的文件路径
 * @mpath: mongodb path,mongodb日志路径
 */
extern void set_log_args( bool dm,
    const char *ppath,const char *epath,const char *mpath);

////////////////////////////////////////////////////////////////////////////////

/* Qt style unused paramter handler */
#define UNUSED(x) (void)x

/* after c++0x,static define in glibc/assert/assert.h */
#if __cplusplus < 201103L    /* -std=gnu99 */
    #define static_assert(x,msg) \
        typedef char __STATIC_ASSERT__##__LINE__[(x) ? 1 : -1]
#endif

/* 分支预测，汇编优化。逻辑上不会改变cond的值 */
#define EXPECT_FALSE(cond) __builtin_expect (!!(cond),0)
#define EXPECT_TRUE(cond)  __builtin_expect (!!(cond),1)

#ifdef _PRINTF_
    #define PRINTF(...) cprintf_log( "CP",__VA_ARGS__ )
    // 线程安全，并且不需要依赖lev的时间
    #define PRINTF_R(...) raw_cprintf_log( ::time(NULL),"CP",__VA_ARGS__ )
#else
    #define PRINTF(...)
    #define PRINTF_R(...)
#endif

#ifdef _ERROR_
    #define ERROR(...) cerror_log( "CE",__VA_ARGS__ )
    // 线程安全，并且不需要依赖lev的时间
    #define ERROR_R(...) raw_cerror_log( ::time(NULL),"CE",__VA_ARGS__ )
#else
    #define ERROR(...)
    #define ERROR_R(...)
#endif

/* terminated without destroying any object and without calling any of the
 * functions passed to atexit or at_quick_exit
 */
#define FATAL(...)    do{ERROR(__VA_ARGS__);::abort();}while(0)

extern void __log_assert_fail (const char *__assertion, const char *__file,
           unsigned int __line, const char *__function);

/* This prints an "log assertion failed" message and return,not abort.  */
#define LOG_ASSERT(why,expr,...)                         \
    do{                                                  \
        if ( !(expr) )                                   \
        {                                                \
            __log_assert_fail (__STRING((why,expr)),     \
                __FILE__, __LINE__, __ASSERT_FUNCTION);  \
            return __VA_ARGS__;                          \
        }                                                \
    }while (0)

#define ARRAY_RESIZE(type,base,cur,cnt,init)        \
    if ( (cnt) > (cur) )                            \
    {                                               \
        uint32_t size = cur > 0 ? cur : 16;           \
        while ( size < (uint32_t)cnt )                \
        {                                           \
            size *= 2;                              \
        }                                           \
        type *tmp = new type[size];                 \
        init( tmp,sizeof(type)*size );              \
        if ( cur > 0)                               \
            memcpy( tmp,base,sizeof(type)*cur );    \
        delete []base;                              \
        base = tmp;                                 \
        cur = size;                                 \
    }

#define MATH_MIN(a,b)    ((a) > (b) ? (b) : (a))
#define MATH_MAX(a,b)    ((a) > (b) ? (a) : (b))

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define ARRAY_NOINIT(base,size)
#define ARRAY_ZERO(base,size)    memset((void *)(base), 0, size)
