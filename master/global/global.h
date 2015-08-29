#ifndef __GOLBAL_H__
#define __GOLBAL_H__

#include <cstdio>      /* c compatible,like printf */
#include <cstdlib>     /* c lib like malloc */
#include <iostream>    /* c++ base support */
#include <ctime>       /* time function */
#include <cstdarg>     /* va_start va_end ... */
#include <unistd.h>    /* POSIX api */
#include <sys/types.h> /* linux sys types like pid_t */
#include <cstring>     /* memset memcpy */
#include <cerrno>      /* errno */

#include "../config.h" /* config paramter */
/* assert with error msg,third parted code not include this file will work fine
   with old assert.
*/
#include "assert.h"
#include "types.h"     /* base data type */
#include "clog.h"      /* log function */

/* Qt style unused paramter handler */
#define UNUSED(x) (void)x

/* 分支预测，汇编优化。逻辑上不会改变cond的值 */
#define expect_false(cond) __builtin_expect (!!(cond),0)
#define expect_true(cond)  __builtin_expect (!!(cond),1)

#ifdef _PDEBUG_
    #define PDEBUG(...)    printf( __VA_ARGS__ )
#else
    #define PDEBUG(...)
#endif

#ifdef _LDEBUG_
    #define LDEBUG(...)    cdebug_log( __VA_ARGS__ )
#else
    #define LDEBUG(...)
#endif

#ifdef _DEBUG_
    #define DEBUG(...)    do{PDEBUG(__VA_ARGS__);LDEBUG(__VA_ARGS__);}while(0)
#else
    #define DEBUG(...)
#endif

#ifdef _ERROR_
    #define ERROR(...)    do{PDEBUG(__VA_ARGS__);cerror_log( __VA_ARGS__ );}while(0)
#else
    #define ERROR(...)
#endif

/* terminated without destroying any object and without calling any of the functions passed to atexit or at_quick_exit */
#define FATAL(...)    do{PDEBUG(__VA_ARGS__);ERROR(__VA_ARGS__);::abort();}while(0)

extern void __log_assert_fail (const char *__assertion, const char *__file,
           unsigned int __line, const char *__function);

/* This prints an "log assertion failed" message and return,not abort.  */
#define log_assert(why,expr,...)                         \
    do{                                                  \
        if ( !(expr) )                                   \
        {                                                \
            __log_assert_fail (__STRING((why,expr)),     \
                __FILE__, __LINE__, __ASSERT_FUNCTION);  \
            return __VA_ARGS__;                          \
        }                                                \
    }while (0)

/* will be called while process exit */
extern void onexit();

#endif  /* __GOLBAL_H__ */
