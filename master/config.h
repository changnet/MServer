#ifndef __CONFIG_H__
#define __CONFIG_H__

/* enable memeory debug */
#define _MEM_DEBUG_

/* print file and time when C++ error */
#define _PFILETIME_

/* print info to stdout with macro PDEBUG */
#define _PDEBUG_

/* write log file with macro LDEBUG */
#define _LDEBUG_

/* write error log */
#define _ERROR_

/* do PDEBUG and LDEBUG both */
#define _DEBUG_

/* cdebug_log file name */
#define CDEBUG_FILE    "cdebuglog"

/* cerror_log file name */
#define CERROR_FILE    "cerrorlog"

/* lua enterance file */
#define LUA_ENTERANCE    "main.lua"

/* is assert work ? */
//#define NDEBUG

/* epoll max events one poll */
#define EPOLL_MAXEV    8192

/* buffer chunk size for socket recv or send */
#define BUFFER_CHUNK    8192
/* 大型buffer缓冲区分界线，采用不同的内存分配策略 */
#define BUFFER_LARGE    65535
/* 小型buffer每次分配的chunk数量 */
#define BUFFER_CHUNK_SIZE 512

/* sql buffer chunk size */
#define SQL_CHUNK    64

/* log file config */
#define LOG_MIN_CHUNK      64
#define LOG_MAX_LENGTH     10240

/* count C++ object which push to lua */
#define OBJ_COUNTER

/* 单个包最大长度，与包头数据类型定义有关 */
#define MAX_PACKET_LEN    65535

//////////////////////////////TCP KEEP ALIVE////////////////////////////////////
/* 是否开启tcp keepalive，一旦开启，所有accept、connect的socket都会启用 */
#define TCP_KEEP_ALIVE

#ifdef TCP_KEEP_ALIVE
# define KEEP_ALIVE_TIME        60    //无通信后60s开始发送
# define KEEP_ALIVE_INTERVAL    10    //间隔10s发送
# define KEEP_ALIVE_PROBES      5     //总共发送5次
#endif
//////////////////////////////TCP KEEP ALIVE////////////////////////////////////

/* 是否开启 TCP_USER_TIMEOUT,一旦开启，所有accept、connect的socket都会启用,内核2.6.37 */
#define _TCP_USER_TIMEOUT    60000    //TCP_USER_TIMEOUT,milliseconds

/* acism 替换缓冲区默认大小 */
#define ACISM_REPLACE_DEFAULT  2048

/* 数据包解析方式 */
#define FLATBUFFERS_PARSE

#endif /* __CONFIG_H__ */
