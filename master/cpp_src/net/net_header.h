#ifndef __NET_HEADER_H__
#define __NET_HEADER_H__

/* 网络通信消息包头格式定义
 */

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

#define SET_LENGTH_FAIL_BOOL do{ return false; } while (0)
#define SET_LENGTH_FAIL_RETURN do{ return -1; } while (0)
#define SET_LENGTH_FAIL_ENCODE \
    do{\
        encoder->finalize();\
        return luaL_error( L,"packet size error" );\
    }while(0)

#define SET_HEADER_LENGTH( h,l,cmd,SET_FAIL )    \
    do{\
        if ( l < 0 ) SET_FAIL;\
        size_t hl = sizeof(h) + l;\
        if (hl > MAX_PACKET_LEN) {\
            ERROR("packet(%d) length(%d) overflow:%s",\
                cmd,(int32)hl,__FUNCTION__);\
            SET_FAIL;\
            return -1;\
        }\
        h._length = static_cast<packet_size_t>(hl);\
    } while (0)

/* 根据一个header指针获取header后buffer的长度 */
#define PACKET_BUFFER_LEN( h )    ((h)->_length - sizeof(*h))


typedef enum
{
    SPKT_NONE = 0,  // invalid
    SPKT_CSPK = 1,  // c2s packet
    SPKT_SCPK = 2,  // s2c packet
    SPKT_SSPK = 3,  // s2s packet
    SPKT_RPCS = 4,  // rpc send packet
    SPKT_RPCR = 5,  // rpc return packet
    SPKT_CBCP = 6,  // client broadcast packet
    SPKT_SBCP = 7,  // server broadcast packet

    SPKT_MAXT       // max packet type
} stream_packet_t;

typedef enum
{
    CLT_MC_NONE  = 0,
    CLT_MC_OWNER = 1, // 根据玩家id广播
    // 未定义的值，会回调到脚本处理

    CLT_MC_MAX
} clt_multicast_t;

#pragma pack (push, 1)

typedef uint16 array_size_t; // 数组长度类型，自定义二进制用。数组最大不超过65535
typedef uint16 packet_size_t; // 包长类型，包最大不超过65535
typedef uint16 string_size_t; // 字符串长度类型，自定义二进制用。最大不超过65535

/* !!!!!!!!! 这里的数据结构必须符合POD结构 !!!!!!!!! */

struct base_header
{
    packet_size_t _length; /* 包长度，包含本身 */
    uint16  _cmd  ; /* 8bit模块号,8bit功能号 */
};

/* 客户端发往服务器 */
struct c2s_header : public base_header
{
};

/* 服务器发往客户端 */
struct s2c_header : public base_header
{
    uint16 _errno ; /* 错误码 */
};

/* 服务器发往服务器 */
struct s2s_header : public base_header
{
    uint16  _errno ; /* 错误码 */
    uint16  _packet; /* 数据包类型，见packet_t */
    uint16  _codec ; /* 解码类型，见codec::codec_t */
    owner_t _owner ; /* 当前数据包所属id，通常为玩家id */
};

#pragma pack(pop)

#endif /* __NET_HEADER_H__ */
