#pragma once

/**
 * @berife 网络通信消息包头格式定义
 */

#include "global/global.hpp"

/*
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
        h.length_ = static_cast<packet_size_t>(hl);                   \
    } while (0)

// 根据一个header指针获取header后buffer的长度
#define PACKET_BUFFER_LEN(h) ((h)->length_ - sizeof(*h))

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
*/

#pragma pack(push, 1)

typedef uint32_t header_size_t; // 包长类型，包最大不超过65535

typedef int32_t h_pid_t;     // 玩家id类型
typedef uint16_t h_list_t;   // 数组长度类型，自定义二进制用。数组最大不超过65535
typedef uint16_t h_string_t; // 字符串长度类型，自定义二进制用。最大不超过65535

inline constexpr int MAX_HEADER_LEN = (1 << sizeof(header_size_t)) - 1;

/* !!!!!!!!! 这里的数据结构必须符合POD结构 !!!!!!!!! */

struct NetHeader
{
    header_size_t size_; /* 包长度，包含本身 */
    uint16_t cmd_;         /* 协议号 */
};

/* 客户端发往服务器 */
struct CSHeader : public NetHeader
{
};

/* 服务器发往客户端 */
struct SCHeader : public NetHeader
{
};

/* 服务器发往服务器 */
struct SSHeader : public NetHeader
{
    uint32_t type_;   /* 数据包类型 */
    h_pid_t pid_;   /* 当前数据包所属玩家id，用于转发数据包 */
};

#pragma pack(pop)
