#ifndef __PACKET_H__
#define __PACKET_H__

/* 网络通信消息包头格式定义
 */

#pragma pack (push, 1)

typedef uint16 array_header;
typedef uint16 packet_length;
typedef uint16 string_header;

/* 客户端发往服务器 */
struct c2s_header
{
    packet_length _length; /* 包长度，不包含本身 */
    uint8  _mod   ; /* 模块号 */
    uint8  _func  ; /* 功能号 */
};

/* 服务器发往客户端 */
struct s2c_header
{
    packet_length _length; /* 包长度，不包含本身 */
    uint8  _mod   ; /* 模块号 */
    uint8  _func  ; /* 功能号 */
    uint16 _errno ; /* 错误码 */
};

/* 服务器发往服务器 */
struct s2s_header
{
    packet_length _length; /* 包长度，不包含本身 */
    uint8  _mod   ; /* 模块号 */
    uint8  _func  ; /* 功能号 */
    uint16 _errno ; /* 错误码 */
};

#pragma pack(pop)


#if defined(__i386__) || defined(__x86_64__)

/* LDR/STR 对应汇编指令LDR/STR */
/* !!! unaligned access,部分arm cpu不支持 !!! */
# define LDR(from,to,type) (to = (*reinterpret_cast<const type *>(from)))
# define STR(to,from,type) ((*reinterpret_cast<type *>(to)) = from)

#else

/* memcpy 在所有平台上都是安全的，但效率稍慢 */
# define LDR(from,to,type) (memcpy( &to,from,sizeof type ))
# define STR(to,from,type) (memcpy( to,&from,sizeof type ))

#endif

#include "net/buffer.h"

struct lua_State;
struct stream_protocol;

class stream_packet
{
public:
    explicit stream_packet( class buffer *buff,lua_State *L )
        : _buff( buff ),L(L)
    {
        _length = 0;
    }
    ~stream_packet() {}

    /* 强制转换为s2c_header s2s_header等 */
    T *operator T*()
    {
        assert( "stream_packet header error",_buff._size + sizeof(T) <= _buff._len );

        return reinterpret_cast<T *>( _buff._size );
    }

    static inline int is_complete( const class buffer *buff )
    {
        uint32 size = _recv.data_size();
        if ( size < sizeof(packet_length) ) return 0;

        packet_length plt = 0;
        LDR( _buff + _pos,plt,T );

        if ( size < sizeof(uint16) + plt ) return 0;

        return  1;
    }

    template< class T >
    int pack( T &header,const struct stream_protocol *proto,int index );
private:
    /* 写入一个基础变量，返回在packet处的位置 */
    template< class T >
    int32 write( const T &val )
    {
        _buff.reserved( sizeof(T),_length );

        int32 pos = _length
        STR( _buff + _buff._size + _length,val,T );
        _length += sizeof(T);

        return pos;
    }

    /* 写入字符串(二进制也当作字符串处理) */
    inline int32 write( const char *ptr,const int32 len )
    {
        assert( "write_string illegal argument",ptr && len > 0 );

        string_header header = static_cast<uint16>( len );
        assert( "string length over UINT16_MAX",header == len );

        _buff.reserved( sizeof(string_header) + len,_length );
        int32 pos = _length;

        /* 先写入长度 */
        STR( _buff + _buff._size + _length,header,string_header );
        _length += sizeof(string_header);

        memcpy( _buff + _buff._size + _length,ptr,len );
        _length += len;

        return pos;
    }

    template < class T >
    int32 read( T &val )
    {
        if ( data_size() < sizeof(T) ){ return -1; }

        LDR( _buff + _pos,val,T );

        _pos += sizeof(T);

        return sizeof(T);
    }
private:
    lua_State *L;
    uint32 _length;
    class buffer *_buff;

    int32 unpack_node( const struct stream_protocol::node *nd );
    int32 pack_node( const struct stream_protocol::node *nd,int32 index );
    int32 pack_element( const struct stream_protocol::node *nd,int32 index );
};

#undef LDR
#undef STR

#endif /* __PACKET_H__ */
