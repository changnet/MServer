#ifndef __PACKET_H__
#define __PACKET_H__

/* 网络通信消息包头格式定义
 */

#include "../global/global.h"

/* 根据一个header指针获取整个packet的长度(包括_length本身) */
#define PACKET_LENGTH( h ) (h->_length + sizeof(packet_length))

/* 根据一个header和buff长度获取header的_length */
#define PACKET_MAKE_LENGTH( h,l )   \
    static_cast<packet_length>(sizeof(h) + l - sizeof(packet_length))

#pragma pack (push, 1)

typedef uint16 array_header;
typedef uint16 packet_length;
typedef uint16 string_header;

/* 客户端发往服务器 */
struct c2s_header
{
    packet_length _length; /* 包长度，不包含本身 */
    uint16  _cmd  ; /* 8bit模块号,8bit功能号 */
};

/* 服务器发往客户端 */
struct s2c_header
{
    packet_length _length; /* 包长度，不包含本身 */
    uint16  _cmd  ; /* 8bit模块号,8bit功能号 */
    uint16 _errno ; /* 错误码 */
};

/* 服务器发往服务器 */
struct s2s_header
{
    packet_length _length; /* 包长度，不包含本身 */
    uint16  _cmd  ; /* 8bit模块号,8bit功能号 */
    uint16 _errno ; /* 错误码 */
    int32    _pid ; /* 玩家id */
};

#pragma pack(pop)

#endif /* __PACKET_H__ */
