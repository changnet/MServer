#include "lstream_socket.h"

#include "ltools.h"
#include "lclass.h"
#include "net/packet.h"
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
    return stream_packet::is_complete( _recv );
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

    luaL_checktype( L,5,LUA_TTABLE );

    const struct stream_protocol::node *nd = (*stream)->find( mod,func );
    if ( (struct stream_protocol::node *)-1 == nd )
    {
        return luaL_error( L,
            "lstream_socket::s2c_send no such protocol %d-%d",mod,func );
    }

    if ( _send.length() > xxx )
    {
        return luaL_error( L,"lstream_socket::s2c_send buffer too large" );
    }

    struct s2c_header header;
    header._mod   = mod;
    header._func  = func;
    header._errno = eno;

     /* 这个函数出错可能不会返回，缓冲区需要能够自动回溯 */
    class stream_packet packet( _send,L );
    if ( packet.pack( header,nd,5 ) < 0 )
    {
        ERROR( "pack_client protocol %d-%d fail",mod,func );
        return luaL_error( L,"pack_client protocol %d-%d fail",mod,func );
    }

    return 0;
}

/* 网络数据不可信，这些数据可能来自非法客户端，有很大机率出错
 * 这个接口少调用luaL_error，尽量保证能返回到lua层处理异常
 * 否则可能导致协议分发接口while循环中止，无法断开非法链接
 */
int32 lstream_socket::unpack_client()
{
    class lstream **stream = static_cast<class lstream **>(
            luaL_checkudata( L, 1, "Stream" ) );

    uint16 size = 0;
    if ( _recv.read( size ) < 0 )
    {
        ERROR( "unpack_client:read size error" );
        return 0;
    }

    uint16 mod  = 0;
    uint16 func = 0;

    if ( _recv.read( mod ) < 0 || _recv.read( func ) < 0 )
    {
        ERROR( "unpack_client:read module or function error:%d-%d",mod,func );
        return 0;
    }

    const struct stream_protocol::node *nd = (*stream)->find( mod,func );
    if ( (struct stream_protocol::node *)-1 == nd )
    {
        ERROR( "lstream_socket::unpack_client no such protocol:%d-%d",mod,func );
        return 0;
    }

    lua_pushinteger( L,mod );
    lua_pushinteger( L,func );

    if ( unpack_node( nd ) < 0 )
    {
        ERROR( "unpack_client: protocol %d-%d error",mod,func );
        return 0;
    }

    return 3;
}
