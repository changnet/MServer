#ifndef __HEADER_INCLUDE_H__
#define __HEADER_INCLUDE_H__

#include "net_include.h"

/* 网络通信消息包头格式定义
 */

/* 根据一个header指针获取整个packet的长度(包括_length本身) */
#define PACKET_LENGTH( h ) ((h)->_length + sizeof(packet_length))

/* 根据一个header和buff长度获取header的_length字段值 */
#define PACKET_MAKE_LENGTH( h,l )   \
    static_cast<packet_length>(sizeof(h) + l - sizeof(packet_length))

/* 根据一个header指针获取header后buffer的长度 */
#define PACKET_BUFFER_LEN( h )    \
    ((h)->_length - sizeof(*h) + sizeof(packet_length))


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
    CLT_MC_ARGS  = 2,  // 根据参数广播

    CLT_MC_MAX
} clt_multicast_t;

#pragma pack (push, 1)

typedef uint16 array_header;
typedef uint16 packet_length;
typedef uint16 string_header;

/* !!!!!!!!! 这里的数据结构必须符合POD结构 !!!!!!!!! */

struct base_header
{
    packet_length _length; /* 包长度，不包含本身 */
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

#endif /* __HEADER_INCLUDE_H__ */
