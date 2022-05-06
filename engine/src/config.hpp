#pragma once

/* 是否打开内存调试，在makefile中定义 */
// #define NMEM_DEBUG
#define NDBG_MEM_TRACE

/* print info to stdout with macro PLOG */
#define _PLOG_

/* write error log */
#define _ELOG_

/* lua enterance file */
#define LUA_ENTERANCE "../src/main.lua"

/* is assert work ? */
//#define NDEBUG


/* 单个包最大长度，与包头数据类型定义有关 */
#define MAX_PACKET_LEN 65535

//////////////////////////////TCP KEEP ALIVE////////////////////////////////////
/* 是否开启tcp keepalive，一旦开启，所有accept、connect的socket都会启用 */
// #define CONF_TCP_KEEP_ALIVE

/* 是否开启
 * TCP_USER_TIMEOUT,一旦开启，所有accept、connect的socket都会启用,内核2.6.37 */
#define CONF_TCP_USER_TIMEOUT 60000 // TCP_USER_TIMEOUT,milliseconds

// 是否启用tcp_nodelay选项
#define CONF_TCP_NODELAY

/* 广播一般有两个用处
 * 1是场景同步广播，这时数量应该是比较小的，目前暂定256。如果太多建议分包广播
 * 2是全服广播，这时用这个函数时传的应该是参数，而不是玩家id。到网关后再从网关获取
 * 玩家连接。
 * 修改这个最大值要考虑栈内存耗尽
 */
#define MAX_CLT_CAST 256

// 格子地图最大的格子数量
#define MAX_MAP_GRID 256

// 是否使用ipv4(默认使用ipv6双栈)
// #define IP_V4 "IPV4"
