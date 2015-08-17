#ifndef __GOLBAL_H__
#define __GOLBAL_H__

#include <cstdio>      /* c compatible,like printf */
#include <iostream>    /* c++ base support */
#include <ctime>       /* time function */
#include <cstdarg>     /* va_start va_end ... */

#include "types.h"     /* base data type */
#include "clog.h"

/* Qt style unused paramter handler */
#define UNUSED(x) (void)x

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

/* time passed caculate,debug function */
extern clock_t tm_start();
extern float tm_stop( const clock_t &start);

#endif  /* __GOLBAL_H__ */
