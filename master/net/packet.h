#ifndef __PACKET_H__
#define __PACKET_H__

/* 网络通信消息包头格式定义
 */

#include <vector>
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
# define LDR(from,to,type) (memcpy( &to,from,sizeof(type) ))
# define STR(to,from,type) (memcpy( to,&from,sizeof(type) ))

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
        _etor = NULL;
    }
    ~stream_packet()
    {
        if ( _etor ) delete _etor;
        _etor = NULL;
    }

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

    const char *last_error();
private:
    /* 写入一个基础变量，返回在packet处的位置 */
    template< class T >
    int32 __attribute__ ((warn_unused_result)) write( const T &val )
    {
        if ( !_buff->reserved( sizeof(T),_length ) ) return -1;

        int32 pos = _length;
        STR( _buff->_buff + _buff->_size + _length,val,T );
        _length += sizeof(T);

        return pos;
    }

    /* 写入字符串(二进制也当作字符串处理) */
    __attribute__ ((warn_unused_result))
    inline int32 write( const char *ptr,const size_t len )
    {
        assert( "write_string illegal argument",ptr && len > 0 );

        string_header header = static_cast<string_header>( len );
        assert( "string length over UINT16_MAX",header == len );

        if ( !_buff->reserved( sizeof(string_header) + len,_length ) ) return -1;
        int32 pos = _length;

        /* 先写入长度 */
        STR( _buff->_buff + _buff->_size + _length,header,string_header );
        _length += sizeof(string_header);

        memcpy( _buff->_buff + _buff->_size + _length,ptr,len );
        _length += len;

        return pos;
    }

    template < class T >
    int32 __attribute__ ((warn_unused_result)) read( T &val )
    {
        const char *buff_pos = _buff->_buff + _buff->_pos - _length;

        assert( "buffer over border",buff_pos > _buff->_buff );

        if (  _length < sizeof(T) ){ return -1; }

        LDR( buff_pos,val,T );

        _length -= sizeof(T);

        return sizeof(T);
    }

    __attribute__ ((warn_unused_result))
    int32 read( char **str,string_header header )
    {
        assert( "packet read string NULL pointer",str );
        char *buff_pos = _buff->_buff + _buff->_pos - _length;

        assert( "buffer over border",buff_pos > _buff->_buff );

        if (  _length < header ){ return -1; }

        *str = buff_pos;

        _length -= header;

        return header;
    }
private:
    const static int32 err_len = 1024;
    struct error_collector
    {
        int32 _func;
        int32 _module;
        char _what[err_len];
        char _message[err_len];
        const char *_function;
        std::vector< std::string > _backtrace;

        error_collector()
        {
            _func   = -1;
            _module = -1;
            _what[0] = 0;
            _message[0] = 0;
            _function = NULL;
        }
    };
private:
    lua_State *L;
    uint32 _length;
    class buffer *_buff;
    struct error_collector *_etor;

    void update_error( const char *fmt,... );
    void append_error( const char *str_err );
    void update_backtrace( int32 mod,int32 func,const char *function );

    int32 unpack_node( const struct stream_protocol::node *nd );
    int32 unpack_element( const struct stream_protocol::node *nd );
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

    _etor->_backtrace.clear(); // 其他数据可以不清，因为不会累积 */
    if ( !_buff->reserved( sizeof(T) ) )
    {
        update_error( "out of buffer" );
        update_backtrace( header._mod,header._func,__PRETTY_FUNCTION__ );
        return -1;
    }

    memcpy( _buff->_buff + _buff->_size,&header,sizeof(T) );
    _length += sizeof(T);

    if ( pack_node( proto,index) < 0 )
    {
        update_backtrace( header._mod,header._func,__PRETTY_FUNCTION__ );
        return -1;
    }

    if ( _length > MAX_PACKET_LEN )
    {
        update_error( "packet over max length" );
        update_backtrace( header._mod,header._func,__PRETTY_FUNCTION__ );
        return -1;
    }

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
    _etor->_backtrace.clear(); // 其他数据可以不清，因为不会累积 */
    int32 old_top = lua_gettop( L );

    /* 先处理长度，这样即使在unpack_node中longjump也不会造成缓冲区数据错误 */
    _length = header._length;
    (_buff->_pos) += sizeof( packet_length ) + _length;

    _length -= sizeof( T ) - sizeof( packet_length );

    /* copy what we need,buffer maybe change when unpack_node */
    int32 module = header._mod;
    int32 fction = header._func;
    if ( unpack_node( proto ) < 0 )
    {
        update_backtrace( module,fction,__PRETTY_FUNCTION__ );
        return -1;
    }

    assert( "unpack protocol stack dirty",
        proto ? old_top + 1 == lua_gettop( L ) : old_top == lua_gettop( L ) );

    return 0;
}

#endif /* __PACKET_H__ */
