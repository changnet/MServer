#include "stream_packet.h"


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

#pragma pack (push, 1)

typedef uint16 array_header;
typedef uint16 packet_length;
typedef uint16 string_header;
typedef int32  owner_t;

/* !!!!!!!!! 这里的数据结构必须符合POD结构 !!!!!!!!! */

struct base_header
{
    packet_length _length; /* 包长度，不包含本身 */
    uint16  _cmd  ; /* 8bit模块号,8bit功能号 */
}

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
    uint16  _mask  ; /* 功能掩码，用于标识转发、广播，见packet_t */
    owner_t _owner ; /* 当前数据包所属id，通常为玩家id */
};

#pragma pack(pop)


stream_packet::stream_packet( class socket *sk )
    : packet( sk )
{
}

stream_packet::~stream_packet()
{
}


int32 stream_packet::pack()
{
    return 0;
}

int32 stream_packet::unpack()
{
    class buffer &recv = _socket->recv_buffer();

    uint32 size = _recv.data_size();
    if ( size < sizeof(base_header) ) return 0;


    return 0;
}
