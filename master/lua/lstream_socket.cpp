#include <lflatbuffers.hpp>

#include "lstream_socket.h"

#include "ltools.h"
#include "lclass.h"
#include "../net/packet.h"
#include "../ev/ev_def.h"

lstream_socket::lstream_socket( lua_State *L )
    : lsocket(L)
{

}

lstream_socket::~lstream_socket()
{
}

/* check if a message packet complete
 * all packet should start with a packet length(uint16)
 */
int32 lstream_socket::is_message_complete()
{
    uint32 sz = _recv.data_size();
    if ( sz < sizeof(packet_length) ) return 0;
    return sz >= *(reinterpret_cast<packet_length *>(_recv.data())) ? 1 : 0;
}

void lstream_socket::listen_cb( int32 revents )
{
    assert( "libev listen cb  error",!(EV_ERROR & revents) );

    while ( socket::active() )
    {
        int32 new_fd = socket::accept();
        if ( new_fd < 0 )
        {
            if ( EAGAIN != errno && EWOULDBLOCK != errno )
            {
                FATAL( "stream_socket::accept fail:%s\n",strerror(errno) );
                return;
            }

            break;  /* 所有等待的连接已处理完 */
        }

        socket::non_block( new_fd );
        KEEP_ALIVE( new_fd );
        USER_TIMEOUT( new_fd );

        /* 直接进入监听 */
        class lstream_socket *_s = new class lstream_socket( L );
        _s->socket::set<lsocket,&lsocket::message_cb>( _s );
        _s->socket::start( new_fd,EV_READ );  /* 这里会设置fd */

        lua_rawgeti(L, LUA_REGISTRYINDEX, ref_acception);
        int32 param = 1;
        if ( ref_self )
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref_self);
            param ++;
        }
        lclass<lstream_socket>::push( L,_s,true );
        if ( expect_false( LUA_OK != lua_pcall(L,param,0,0) ) )
        {
            /* 如果失败，会出现几种情况：
             * 1) lua能够引用socket，只是lua其他逻辑出错.需要lua层处理此异常
             * 2) lua不能引用socket，导致lua层gc时会销毁socket,ev还来不及fd_reify，又
             *    将此fd删除，触发一个错误
             */
            ERROR( "listen cb call accept handler fail:%s",lua_tostring(L,-1) );
            return;
        }
    }
}

/* ssc_flatbuffers_send( lfb,srv_msg,clt_msg,schema,object,tbl ) */
int lstream_socket::ssc_flatbuffers_send()
{
    class lflatbuffers** lfb =
        (class lflatbuffers**)luaL_checkudata( L, 1, "lua_flatbuffers" );
    if ( lfb == NULL || *lfb == NULL )
    {
        return luaL_error( L, "argument #1 expect lua_flatbuffers" );
    }

    int32 srv_msg = luaL_checkinteger( L,2 );
    int32 clt_msg = luaL_checkinteger( L,3 );

    const char *schema = luaL_checkstring( L,4 );
    const char *object = luaL_checkstring( L,5 );

    if ( !lua_istable( L,6 ) )
    {
        return luaL_error( L,
            "argument #6 expect table,got %s",lua_typename( L,lua_type(L,6) ) );
    }

    if ( (*lfb)->encode( L,schema,object,6 ) < 0 )
    {
        return luaL_error( L,(*lfb)->last_error() );
    }

    size_t sz = 0;
    const char *buffer = (*lfb)->get_buffer( sz );
    if ( sz > USHRT_MAX )
    {
        return luaL_error( L,"buffer size over USHRT_MAX" );
    }

    if ( !_send.reserved( sz + sizeof(struct s2c_header) + sizeof(struct s2s_header) ) )
    {
        return luaL_error( L,"out of socket buffer" );
    }

    struct s2c_header s2ch;
    s2ch._length = static_cast<packet_length>(
        sz + sizeof(struct s2c_header) - sizeof(packet_length) );
    s2ch._cmd    = static_cast<uint16>  ( clt_msg );

    struct s2s_header s2sh;
    s2sh._length = static_cast<packet_length>( sz +
        sizeof(struct s2c_header) + sizeof(struct s2s_header) - sizeof(packet_length) );
    s2sh._cmd    = static_cast<uint16>  ( srv_msg );

    _send.__append( &s2sh,sizeof(struct s2s_header) );
    _send.__append( &s2ch,sizeof(struct s2c_header) );
    _send.__append( buffer,sz );

    pending_send();
    return 0;
}


/* sc_flatbuffers_send( lfb,clt_msg,schema,object,tbl ) */
int lstream_socket::sc_flatbuffers_send()
{
    class lflatbuffers** lfb =
        (class lflatbuffers**)luaL_checkudata( L, 1, "lua_flatbuffers" );
    if ( lfb == NULL || *lfb == NULL )
    {
        return luaL_error( L, "argument #1 expect lua_flatbuffers" );
    }

    int32 clt_msg = luaL_checkinteger( L,2 );

    const char *schema = luaL_checkstring( L,3 );
    const char *object = luaL_checkstring( L,4 );

    if ( !lua_istable( L,5 ) )
    {
        return luaL_error( L,
            "argument #5 expect table,got %s",lua_typename( L,lua_type(L,5) ) );
    }

    if ( (*lfb)->encode( L,schema,object,6 ) < 0 )
    {
        return luaL_error( L,(*lfb)->last_error() );
    }

    size_t sz = 0;
    const char *buffer = (*lfb)->get_buffer( sz );
    if ( sz > USHRT_MAX )
    {
        return luaL_error( L,"buffer size over USHRT_MAX" );
    }

    if ( !_send.reserved( sz + sizeof(struct s2c_header) + sizeof(struct s2s_header) ) )
    {
        return luaL_error( L,"out of socket buffer" );
    }

    struct s2c_header s2ch;
    s2ch._length = static_cast<packet_length>(
        sz + sizeof(struct s2c_header) - sizeof(packet_length) );
    s2ch._cmd    = static_cast<uint16>  ( clt_msg );

    _send.__append( &s2ch,sizeof(struct s2c_header) );
    _send.__append( buffer,sz );

    pending_send();
    return 0;
}

 /* server to server:ss_flatbuffers_send( lfb,msg,schema,object,tbl ) */
int lstream_socket::ss_flatbuffers_send()
{
    class lflatbuffers** lfb =
        (class lflatbuffers**)luaL_checkudata( L, 1, "lua_flatbuffers" );
    if ( lfb == NULL || *lfb == NULL )
    {
        return luaL_error( L, "argument #1 expect lua_flatbuffers" );
    }

    int32 srv_msg = luaL_checkinteger( L,2 );

    const char *schema = luaL_checkstring( L,3 );
    const char *object = luaL_checkstring( L,4 );

    if ( !lua_istable( L,5 ) )
    {
        return luaL_error( L,
            "argument #6 expect table,got %s",lua_typename( L,lua_type(L,5) ) );
    }

    if ( (*lfb)->encode( L,schema,object,5 ) < 0 )
    {
        return luaL_error( L,(*lfb)->last_error() );
    }

    size_t sz = 0;
    const char *buffer = (*lfb)->get_buffer( sz );
    if ( sz > USHRT_MAX )
    {
        return luaL_error( L,"buffer size over USHRT_MAX" );
    }

    if ( !_send.reserved( sz + sizeof(struct s2s_header) ) )
    {
        return luaL_error( L,"out of socket buffer" );
    }

    struct s2s_header s2sh;
    s2sh._length = static_cast<packet_length>(
        sz + sizeof(struct s2s_header) - sizeof(packet_length) );
    s2sh._cmd    = static_cast<uint16>  ( srv_msg );

    _send.__append( &s2sh,sizeof(struct s2s_header) );
    _send.__append( buffer,sz );

    pending_send();
    return 0;
}

/* decode server to server message:ss_flatbuffers_decode( lfb,srv_msg,schema,object ) */
int lstream_socket::ss_flatbuffers_decode()
{
    class lflatbuffers** lfb =
        (class lflatbuffers**)luaL_checkudata( L, 1, "lua_flatbuffers" );
    if ( lfb == NULL || *lfb == NULL )
    {
        return luaL_error( L, "argument #1 expect lua_flatbuffers" );
    }

    int32 srv_msg = luaL_checkinteger( L,2 );

    const char *schema = luaL_checkstring( L,3 );
    const char *object = luaL_checkstring( L,4 );

    uint32 sz = _recv.data_size();
    if ( sz < sizeof(s2s_header) )
    {
        return luaL_error( L, "incomplete message header" );
    }

    struct s2s_header *ph = reinterpret_cast<struct s2s_header *>(_recv.data());

    /* 验证包长度，_length并不包含本身 */
    size_t len = ph->_length + sizeof( packet_length );
    if ( sz < len )
    {
        return luaL_error( L, "incomplete message header" );
    }

    /* 协议号是否匹配 */
    if ( srv_msg != ph->_cmd )
    {
        return luaL_error( L,
            "message valid fail,expect %d,got %d",srv_msg,ph->_cmd );
    }

    /* 删除buffer,避免luaL_error longjump影响 */
    _recv.subtract( len );
    const char *buffer = _recv.data() + sizeof( struct s2s_header );

    if ( (*lfb)->decode( L,schema,object,buffer,len ) < 0 )
    {
        return luaL_error( L,(*lfb)->last_error() );
    }

    return 1;
}
