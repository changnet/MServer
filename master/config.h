#ifndef __CONFIG_H__
#define __CONFIG_H__

/* print info to stdout with macro PDEBUG */
#define _PDEBUG_

/* write log file with macro LDEBUG */
#define _LDEBUG_

/* write error log */
#define _ERROR_

/* do PDEBUG and LDEBUG both */
#define _DEBUG_

/* do PDEBUG and write log to runtime */
#define _RUNTIME_

/* cdebug_log file name */
#define CDEBUG_FILE    "cdebuglog"

/* cerror_log file name */
#define CERROR_FILE    "cerrorlog"

/* runtime file name */
#define CRUNTIME_FILE  "cruntimelog"

/* lua enterance file */
#define LUA_ENTERANCE    "main.lua"

/* lua preload file */
#define LUA_PRELOAD     "preload.lua"

/* is assert work ? */
//#define NDEBUG

/* epoll max events one poll */
#define EPOLL_MAXEV    8192

#endif /* __CONFIG_H__ */
