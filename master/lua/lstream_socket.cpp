#include <lflatbuffers.hpp>

#include "lstream_socket.h"

#include "ltools.h"
#include "lclass.h"
#include "lstream.h"
#include "../net/packet.h"
#include "../ev/ev_def.h"

lstream_socket::lstream_socket( lua_State *L )
    : lsocket(L)
{

}

lstream_socket::~lstream_socket()
{
}

int32 lstream_socket::is_message_complete()
{
    return stream_packet::is_complete( &_recv );
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

int32 lstream_socket::s2c_send()
{
    class lstream **stream = static_cast<class lstream **>(
        luaL_checkudata( L, 1, "Stream" ) );
    uint16 mod  = static_cast<uint16>( luaL_checkinteger( L,2 ) );
    uint16 func = static_cast<uint16>( luaL_checkinteger( L,3 ) );
    uint16 eno  = static_cast<uint16>( luaL_checkinteger( L,4 ) );

    const struct stream_protocol::node *nd = (*stream)->find( mod,func );
    if ( (struct stream_protocol::node *)-1 == nd )
    {
        return luaL_error( L,
            "lstream_socket::s2c_send no such protocol %d-%d",mod,func );
    }
    else if ( nd ) /* 为0表示空协议，可以不传table */
    {
        luaL_checktype( L,5,LUA_TTABLE );
    }

    struct s2c_header header;
    header._mod   = mod;
    header._func  = func;
    header._errno = eno;

     /* 这个函数出错可能不会返回，缓冲区需要能够自动回溯 */

    int32 result = 0;
    const char *str_err = NULL;
    {
        class stream_packet packet( &_send,L );
        if ( (result = packet.pack( header,nd,5 ) ) < 0 )
        {
            str_err = packet.last_error();
            ERROR( str_err );
        }
    }

    /* stack-unwind和longjump冲突
     * 调用luaL_error,需要用code-block保证packet析构函数能执行
     * header这个不需要在析构函数里运行逻辑，无所谓
     */
    if ( result < 0 ) return luaL_error( L,str_err );

    pending_send();

    return 0;
}

/* 网络数据不可信，这些数据可能来自非法客户端，有很大机率出错
 * 这个接口少调用luaL_error，尽量保证能返回到lua层处理异常
 * 否则可能导致协议分发接口while循环中止，无法断开非法链接
 */
int32 lstream_socket::c2s_recv()
{
    class lstream **stream = static_cast<class lstream **>(
            luaL_checkudata( L, 1, "Stream" ) );

    if ( !stream_packet::is_complete( &_recv ) )
    {
        lua_pushinteger( L,0 );
        return 1;
    }

    c2s_header *header = NULL;
    if ( _recv.data_size() < sizeof(c2s_header) )
    {
        lua_pushinteger( L,0 );
        return 1;
    }

    header = reinterpret_cast<c2s_header*>( _recv.data() );

    const struct stream_protocol::node *nd = (*stream)->find( header->_mod,header->_func );
    if ( (struct stream_protocol::node *)-1 == nd )
    {
        ERROR( "c2s_recv no such protocol:%d-%d",header->_mod,header->_func );
        lua_pushinteger( L,-1 );
        return 1;
    }

    lua_pushinteger( L,header->_length );
    lua_pushinteger( L,header->_mod );
    lua_pushinteger( L,header->_func );

    class stream_packet packet( &_recv,L );
    if ( packet.unpack( *header,nd ) < 0 )
    {
        ERROR( packet.last_error() );

        lua_pushinteger( L,-1 );
        return 1;
    }

    /* 如果这时缓冲区刚好是空的，尽快处理悬空区，这时代价最小，不用拷贝内存 */
    if ( _recv.data_size() <= 0 ) _recv.clear();

    return 4;
}

int32 lstream_socket::c2s_send()
{
    class lstream **stream = static_cast<class lstream **>(
        luaL_checkudata( L, 1, "Stream" ) );
    uint16 mod  = static_cast<uint16>( luaL_checkinteger( L,2 ) );
    uint16 func = static_cast<uint16>( luaL_checkinteger( L,3 ) );

    const struct stream_protocol::node *nd = (*stream)->find( mod,func );
    if ( (struct stream_protocol::node *)-1 == nd )
    {
        return luaL_error( L,
            "lstream_socket::s2c_send no such protocol %d-%d",mod,func );
    }
    else if ( nd )
    {
        luaL_checktype( L,4,LUA_TTABLE );
    }

    struct c2s_header header;
    header._mod   = mod;
    header._func  = func;

    /* 这个函数出错可能不会返回，缓冲区需要能够自动回溯 */
    int result = 0;
    const char *str_err = NULL;
    {
        class stream_packet packet( &_send,L );
        if ( (result = packet.pack( header,nd,4 ) ) < 0 )
        {
            str_err = packet.last_error();
            ERROR( str_err );
        }
    }

    /* stack-unwind和longjump冲突
     * 调用luaL_error,需要用code-block保证packet析构函数能执行
     * header这个不需要在析构函数里运行逻辑，无所谓
     */
    if ( result < 0 ) return luaL_error( L,str_err );

    pending_send();

    return 0;
}

/* 网络数据不可信，这些数据可能来自非法客户端，有很大机率出错
 * 这个接口少调用luaL_error，尽量保证能返回到lua层处理异常
 * 否则可能导致协议分发接口while循环中止，无法断开非法链接
 */
int32 lstream_socket::s2c_recv()
{
    class lstream **stream = static_cast<class lstream **>(
            luaL_checkudata( L, 1, "Stream" ) );

    if ( !stream_packet::is_complete( &_recv ) )
    {
        lua_pushinteger( L,0 );
        return 1;
    }

    s2c_header *header = NULL;
    if ( _recv.data_size() < sizeof(s2c_header) )
    {
        lua_pushinteger( L,0 );
        return 1;
    }

    header = reinterpret_cast<s2c_header*>( _recv.data() );

    const struct stream_protocol::node *nd = (*stream)->find( header->_mod,header->_func );
    if ( (struct stream_protocol::node *)-1 == nd )
    {
        ERROR( "s2c_recv:no such protocol:%d-%d",header->_mod,header->_func );
        lua_pushinteger( L,-1 );
        return 1;
    }

    lua_pushinteger( L,header->_length );
    lua_pushinteger( L,header->_mod );
    lua_pushinteger( L,header->_func );

    class stream_packet packet( &_recv,L );
    if ( packet.unpack( *header,nd ) < 0 )
    {
        ERROR( packet.last_error() );

        lua_pushinteger( L,-1 );
        return 1;
    }

    /* 如果这时缓冲区刚好是空的，尽快处理悬空区，这时代价最小，不用拷贝内存 */
    if ( _recv.data_size() <= 0 ) _recv.clear();

    return 4;
}

/* s2c_flatbuffers_send( lfb,srv_msg,clt_msg,schema,object,tbl ) */
int lstream_socket::s2c_flatbuffers_send()
{
    class lflatbuffers** lfb =
        (class lflatbuffers**)luaL_checkudata( L, 1, "lua_flatbuffers" );
    if ( lfb == NULL || *lfb == NULL )
    {
        return luaL_error( L, "s2c_flatbuffers_send argument #1 expect lua_flatbuffers" );
    }

    int32 srv_msg = luaL_checkinteger( L,2 );
    int32 clt_msg = luaL_checkinteger( L,3 );

    const char *schema = luaL_checkstring( L,4 );
    const char *object = luaL_checkstring( L,5 );

    if ( !lua_istable( L,6 ) )
    {
        return luaL_error( L,
            "s2c_flatbuffers_send argument #6 expect table,got %s",
            lua_typename( L,lua_type(L,6) ) );
    }

    if ( (*lfb)->encode( L,schema,object,6 ) < 0 )
    {
        return luaL_error( L,(*lfb)->last_error() );
    }

    size_t sz = 0;
    const char *buffer = (*lfb)->get_buffer( sz );
    return 0;
}
