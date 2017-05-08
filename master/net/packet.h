#ifndef __PACKET_H__
#define __PACKET_H__

/* 网络通信消息包头格式定义
 */

#include "../global/global.h"

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
    uint16  _cmd   ; /* 8bit模块号,8bit功能号 */
    uint16  _errno ; /* 错误码 */
    uint16  _mask  ; /* 功能掩码，用于标识转发、广播，见packet_t */
    owner_t _owner ; /* 当前数据包所属id，通常为玩家id */
};

#pragma pack(pop)

struct lua_State;
class lflatbuffers;

class packet
{
public:
    typedef enum
    {
        PKT_NONE = 0,  // invalid
        PKT_CSPK = 1,  // c2s packet
        PKT_SCPK = 2,  // s2c packet
        PKT_SSPK = 3,  // s2s packet
        PKT_RPCS = 4,  // rpc send packet
        PKT_RPCR = 5,  // rpc return packet
        PKT_CBCP = 6,  // client broadcast packet
        PKT_SBCP = 7,  // server broadcast packet

        PKT_MAXT       // max packet type
    } packet_t;
public:
    static void uninstance();
    static class packet *instance();

    /* 加载path目录下所有schema文件 */
    int32 load_schema( const char *path );

    /* 外部解析接口 */
    int32 parse( lua_State *L,
        const char *schema,const char *object,const c2s_header *header );
    int32 parse( lua_State *L,
        const char *schema,const char *object,const s2s_header *header );
    int32 parse( lua_State *L,
        const char *schema,const char *object,const s2c_header *header );
    int32 parse( lua_State *L,const s2s_header *header );

    /* c2s打包接口 */
    int32 unparse_c2s( lua_State *L,int32 index,
        int32 cmd,const char *schema,const char *object,class buffer &send );
    /* s2c打包接口 */
    int32 unparse_s2c( lua_State *L,int32 index,int32 cmd,
        int32 ecode,const char *schema,const char *object,class buffer &send );
    /* s2s打包接口 */
    int32 unparse_s2s( lua_State *L,int32 index,int32 session,int32 cmd,
        int32 ecode,const char *schema,const char *object,class buffer &send );
    /* ssc打包接口 */
    int32 unparse_ssc( lua_State *L,int32 index,owner_t owner,int32 cmd,
        int32 ecode,const char *schema,const char *object,class buffer &send );
    /* rpc调用打包接口 */
    int32 unparse_rpc( lua_State *L,
        int32 unique_id,int32 index,class buffer &send );
    /* rpc返回打包接口 */
    int32 unparse_rpc( lua_State *L,
        int32 unique_id,int32 ecode,int32 index,class buffer &send );
private:
    packet();
    ~packet();
private:
    static class packet *_packet;
    class lflatbuffers  *_lflatbuffers;
};

#endif /* __PACKET_H__ */
