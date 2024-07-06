#pragma once

/* 网络通信消息包头格式定义
 */

#include "global/global.hpp"

#define MAX_SCHEMA_NAME 64

typedef int32_t Owner;

#define SET_LENGTH_FAIL_BOOL \
    do                       \
    {                        \
        return false;        \
    } while (0)
#define SET_LENGTH_FAIL_RETURN \
    do                         \
    {                          \
        return -1;             \
    } while (0)
#define SET_LENGTH_FAIL_ENCODE                     \
    do                                             \
    {                                              \
        encoder->finalize();                       \
        return luaL_error(L, "packet size error"); \
    } while (0)

#define SET_HEADER_LENGTH(h, l, cmd, SET_FAIL)                        \
    do                                                                \
    {                                                                 \
        if ((int32_t)l < 0) SET_FAIL;                                 \
        size_t hl = sizeof(h) + l;                                    \
        if (hl > MAX_PACKET_LEN)                                      \
        {                                                             \
            ELOG("packet(%d) length(%d) overflow", cmd, (int32_t)hl); \
            SET_FAIL;                                                 \
            return -1;                                                \
        }                                                             \
        h._length = static_cast<packet_size_t>(hl);                   \
    } while (0)

/* 根据一个header指针获取header后buffer的长度 */
#define PACKET_BUFFER_LEN(h) ((h)->_length - sizeof(*h))

typedef enum
{
    SPT_NONE = 0, // invalid
    SPT_CSPK = 1, // c2s packet
    SPT_SCPK = 2, // s2c packet
    SPT_SSPK = 3, // s2s packet
    SPT_RPCS = 4, // rpc send packet
    SPT_RPCR = 5, // rpc return packet
    SPT_CBCP = 6, // client broadcast packet
    SPT_SBCP = 7, // server broadcast packet

    SPT_MAXT // max packet type
} StreamPacketType;

typedef enum
{
    CLT_MC_NONE  = 0,
    CLT_MC_OWNER = 1, // 根据玩家id广播
    // 未定义的值，会回调到脚本处理

    CLT_MC_MAX
} clt_multicast_t;

#pragma pack(push, 1)

typedef uint16_t array_size_t; // 数组长度类型，自定义二进制用。数组最大不超过65535
typedef uint16_t packet_size_t; // 包长类型，包最大不超过65535
typedef uint16_t string_size_t; // 字符串长度类型，自定义二进制用。最大不超过65535

/* !!!!!!!!! 这里的数据结构必须符合POD结构 !!!!!!!!! */

struct base_header
{
    packet_size_t _length; /* 包长度，包含本身 */
    uint16_t _cmd;         /* 协议号 */
};

/* 客户端发往服务器 */
struct c2s_header : public base_header
{
};

/* 服务器发往客户端 */
struct s2c_header : public base_header
{
};

/* 服务器发往服务器 */
struct s2s_header : public base_header
{
    uint16_t _packet; /* 数据包类型，见packet_t */
    Owner _owner;     /* 当前数据包所属id，通常为玩家id */
};

#pragma pack(pop)
