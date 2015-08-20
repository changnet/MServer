#ifndef __GOLBAL_H__
#define __GOLBAL_H__

#include <cstdio>      /* c compatible,like printf */
#include <cstdlib>     /* c lib like malloc */
#include <iostream>    /* c++ base support */
#include <ctime>       /* time function */
#include <cstdarg>     /* va_start va_end ... */
#include <unistd.h>    /* POSIX api */
#include <sys/types.h> /* linux sys types like pid_t */

#include "../config.h" /* config paramter */
/* assert with error msg,third parted code not include this file will work fine
   with old assert.
*/
#include "assert.h"
#include "types.h"     /* base data type */
#include "clog.h"      /* log function */

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

#ifdef _DEBUG_
    #define DEBUG(...)    do{PDEBUG(__VA_ARGS__);LDEBUG(__VA_ARGS__);}while(0)
#else
    #define DEBUG(...)
#endif

/* do not define ERROR,conflict with libev */
#ifdef _ERROR_
    #define ELOG(...)    cerror_log( __VA_ARGS__ )
#else
    #define ELOG(...)
#endif

#define FATAL(...)    do{PDEBUG(__VA_ARGS__);ELOG(__VA_ARGS__);}while(0)

/* will be called while process exit */
extern void onexit();

#endif  /* __GOLBAL_H__ */
