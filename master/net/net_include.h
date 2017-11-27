#ifndef __NET_INCLUDE_H__
#define __NET_INCLUDE_H__

#include "../global/global.h"

#define MAX_SCHEMA_NAME  64

/* _maks 设定掩码，按位
 * 1bit: 解码方式:0 普通解码，1 unpack解码
 * 2bit: TODO:广播方式 ？？这个放到脚本
 */
struct cmd_cfg_t
{
    int32 _cmd;
    int32 _mask;
    int32 _session;
    char _schema[MAX_SCHEMA_NAME];
    char _object[MAX_SCHEMA_NAME];
};
typedef int32  owner_t;

#endif /* __NET_INCLUDE_H__ */