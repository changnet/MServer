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
    uint16 _cmd   ; /* 8bit模块号,8bit功能号 */
    uint16 _errno ; /* 错误码 */
    int32  _pid   ; /* 玩家id */
    uint16 _mask  ; /* 功能掩码 */
};

#pragma pack(pop)

class packet
{
public:
    typedef enum
    {
        PKT_NONE = 0,  // invalid
        PKT_CSPK = 1,  // c2s packet
        PKT_SCPK = 2,  // s2c packet
        PKT_SSPK = 3,  // s2s packet
        PKT_RPCP = 4,  // rpc packet
        PKT_CBCP = 5,  // client broadcast packet
        PKT_SBCP = 6,  // server broadcast packet

        PKT_MAXT       // max packet type
    } packet_t;
public:
    /* 外部解析接口 */
    static void parse_header( const char *buffer,const c2s_header *header );
    static void parse_header( const char *buffer,const s2c_header *header );
    static void parse_header( const char *buffer,const s2s_header *header );
private:
    /* 内部解析接口，根据不同解析方式实现 */
    static void do_parse_header( const char *buffer,const c2s_header *header );
    static void do_parse_header( const char *buffer,const s2c_header *header );
    static void do_parse_header( const char *buffer,const s2s_header *header );
};

#endif /* __PACKET_H__ */
