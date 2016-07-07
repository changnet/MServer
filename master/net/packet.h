#ifndef __PACKET_H__
#define __PACKET_H__

/* 网络通信消息包头格式定义
 */

#include "../global/global.h"

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

#include "../net/buffer.h"
#include "../stream/stream_protocol.h"

struct lua_State;

class stream_packet
{
public:
    explicit stream_packet( class buffer *buff,lua_State *L )
        : L(L),_length(0),_buff( buff )
    {
    }
    ~stream_packet() {}

    static inline int32 is_complete( const class buffer *buff )
    {
        uint32 size = buff->data_size();
        if ( size < sizeof(packet_length) ) return 0;

        packet_length plt = 0;
        LDR( buff->data(),plt,packet_length );

        /* 长度不包括本身 */
        if ( size < sizeof(packet_length) + plt ) return 0;

        return  1;
    }

    template< class T >
    int pack( T &header,const struct stream_protocol::node *proto,int32 index );

    template< class T >
    int unpack( T &header,const struct stream_protocol::node *proto );
private:
    /* 写入一个基础变量，返回在packet处的位置 */
    template< class T >
    int32 write( const T &val )
    {
        _buff->reserved( sizeof(T),_length );

        int32 pos = _length;
        STR( _buff->_buff + _buff->_size + _length,val,T );
        _length += sizeof(T);

        return pos;
    }

    /* 写入字符串(二进制也当作字符串处理) */
    inline int32 write( const char *ptr,const int32 len )
    {
        assert( "write_string illegal argument",ptr && len > 0 );

        string_header header = static_cast<uint16>( len );
        assert( "string length over UINT16_MAX",header == len );

        _buff->reserved( sizeof(string_header) + len,_length );
        int32 pos = _length;

        /* 先写入长度 */
        STR( _buff + _buff->_size + _length,header,string_header );
        _length += sizeof(string_header);

        memcpy( _buff + _buff->_size + _length,ptr,len );
        _length += len;

        return pos;
    }

    template < class T >
    int32 read( T &val )
    {
        if ( _buff->data_size() < sizeof(T) ){ return -1; }

        LDR( _buff->_buff + _buff->_pos - _length,val,T );

        _length -= sizeof(T);

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

template< class T >
int stream_packet::pack( T &header,const struct stream_protocol::node *proto,int32 index )
{
    assert( "packet length dirty",0 == _length );
    assert( "empty packet",_buff && L );

    _buff->reserved( sizeof(T) );
    memcpy( _buff->data(),&header,sizeof(T) );
    _length += sizeof(T);

    if ( pack_node( proto,index) < 0 ) return -1;

    /* 更新包长度，不包括自身长度 */
    packet_length length = _length - sizeof(packet_length);
    assert( "packet length zero",length > 0 );

    memcpy( _buff->_buff + _buff->_size,&length,sizeof(packet_length) );
    _buff->_size += _length;

    return 0;
}

template< class T >
int stream_packet::unpack( T &header,const struct stream_protocol::node *proto )
{
    int32 old_top = lua_gettop( L );

    /* 先处理长度，这样即使在unpack_node中longjump也不会造成缓冲区数据错误 */
    _length = header->_length;
    (_buff->_pos) += sizeof( packet_length ) + _length;
    if ( unpack_node( proto ) < 0 )
    {
        return -1;
    }

    assert( "unpack protocol stack dirty",
        proto ? old_top + 1 == lua_gettop( L ) : old_top == lua_gettop( L ) );

    return 0;
}

#endif /* __PACKET_H__ */
