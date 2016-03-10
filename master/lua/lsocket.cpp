#include "lsocket.h"
#include "ltools.h"
#include "../ev/ev_def.h"

lsocket::lsocket( lua_State *L )
    : L(L)
{
    ref_self       = 0;
    ref_message    = 0;
    ref_acception  = 0;
    ref_connection = 0;
    ref_disconnect = 0;
}

lsocket::~lsocket()
{
    _socket.stop(); /* lsocket的内存由lua控制，保证在释放socket时一定会关闭 */

    /* 释放引用，如果有内存问题，可查一下这个地方 */
    LUA_UNREF( ref_self       );
    LUA_UNREF( ref_message    );
    LUA_UNREF( ref_acception  );
    LUA_UNREF( ref_connection );
    LUA_UNREF( ref_disconnect );
}

/* 仅关闭socket，但不销毁内存 */
int32 lsocket::kill()
{
    /* stop尝试把缓冲区的数据直接发送 */
    _socket.stop();

    return 0;
}

int32 lsocket::listen()
{
    if ( _socket.active() )
    {
        return luaL_error( L,"listen:socket already active");
    }

    const char *host = luaL_checkstring( L,1 );
    if ( !host )
    {
        return luaL_error( L,"listen:address not specify" );
    }

    int32 port = luaL_checkinteger( L,2 );
    int32 fd = _socket.listen( host,port );
    if ( fd < 0 )
    {
        ERROR( "socket listen fail:%s\n",strerror(errno) );
        return 0;
    }

    _socket.set<lsocket,&lsocket::listen_cb>( this );
    _socket.start( fd,EV_READ );

    lua_pushinteger( L,fd );
    return 1;
}

int32 lsocket::connect()
{
    if ( _socket.active() )
    {
        return luaL_error( L,"connect:socket already active");
    }

    const char *host = luaL_checkstring( L,1 );
    if ( !host )
    {
        return luaL_error( L,"connect:host not specify" );
    }

    int32 port = luaL_checkinteger( L,2 );

    int32 fd = _socket.connect( host,port );
    if ( fd < 0 )
    {
        ERROR( "socket connect fail:%s\n",strerror(errno) );
        return 0;
    }

    _socket.set<lsocket,&lsocket::connect_cb>( this );
    _socket.start( fd,EV_WRITE ); /* write事件 */

    lua_pushinteger( L,fd );
    return 1;
}

/* 发送原始数据，二进制或字符串 */
int32 lsocket::send()
{
    if ( !_socket.active() )
    {
        return luaL_error( L,"socket::send not valid");
    }

    size_t len = 0;
    const char *sz = luaL_checklstring( L,1,&len );

    if ( !sz || len <= 0 )
    {
        return luaL_error( L,"socket::send nothing to send" );
    }

    _socket.append( sz,len );

    return 0;
}

void lsocket::message_cb( int32 revents )
{
    assert( "libev read cb error",!(EV_ERROR & revents) );

    /* 就游戏中的绝大多数消息而言，一次recv就能接收完成，不需要while接收直到出错。而且
     * 当前设定的缓冲区与socket一致(8192)，socket缓冲区几乎不可能还有数据，不需要多调用
     * 一次recv。退一步，假如还有数据，epoll当前为LT模式，下一次回调再次读取
     * 如果启用while,需要检测_socket在lua层逻辑中是否被关闭，防止自杀的情况
     */

    int32 ret = _socket.recv();

    /* disconnect or error */
    if ( 0 == ret )
    {
        on_disconnect();
        return;
    }
    else if ( ret < 0 )
    {
        if ( EAGAIN != errno && EWOULDBLOCK != errno )
        {
            ERROR( "socket message_cb error:%s\n",strerror(errno) );
            on_disconnect();
        }
        return;
    }

    /* 此框架中，socket的内存由lua管理，无法预知lua会何时释放内存
     * 因此不要在C++层用while去解析协议
     */
    if ( !is_message_complete() ) return;

    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_message);
    int32 param = 0;
    if ( ref_self )
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref_self);
        param ++;
    }
    if ( expect_false( LUA_OK != lua_pcall(L,param,0,0) ) )
    {
        ERROR( "message_notify fail:%s\n",lua_tostring(L,-1) );
        return;
    }
}

void lsocket::on_disconnect()
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_disconnect);
    int32 param = 0;
    if ( ref_self )
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref_self);
        param ++;
    }
    if ( expect_false( LUA_OK != lua_pcall(L,param,0,0) ) )
    {
        ERROR( "socket disconect,call lua fail:%s\n",lua_tostring(L,-1) );
        // DO NOT RETURN
    }

    /* 关闭fd，但不要delete
     * 先回调lua，再close.lua可能会调用一些函数，如取fd
     */
    _socket.stop();
}

int32 lsocket::address()
{
    if ( !_socket.active() )
    {
        return luaL_error( L,"socket::address not a active socket" );
    }

    lua_pushstring( L,_socket.address() );
    return 1;
}

int32 lsocket::set_self_ref()
{
    if ( !lua_istable( L,1 ) )
    {
        return luaL_error( L,"set_self_ref,argument illegal.expect table" );
    }

    LUA_REF( ref_self );

    return 0;
}

int32 lsocket::set_on_message()
{
    if ( !lua_isfunction( L,1 ) )
    {
        return luaL_error( L,"set_on_message,argument illegal.expect function" );
    }

    LUA_REF( ref_message );

    return 0;
}

int32 lsocket::set_on_acception()
{
    if ( !lua_isfunction( L,1 ) )
    {
        return luaL_error( L,"set_on_acception,argument illegal.expect function" );
    }

    LUA_REF( ref_acception );

    return 0;
}

int32 lsocket::set_on_connection()
{
    if ( !lua_isfunction( L,1 ) )
    {
        return luaL_error( L,"set_on_connection,argument illegal.expect function" );
    }

    LUA_REF( ref_connection );

    return 0;
}

int32 lsocket::set_on_disconnect()
{
    if ( !lua_isfunction( L,1 ) )
    {
        return luaL_error( L,"set_on_disconnect,argument illegal.expect function" );
    }

    LUA_REF( ref_disconnect );

    return 0;
}

int32 lsocket::file_description()
{
    lua_pushinteger( L,_socket.fd() );
    return 1;
}

/*
 * connect回调
 * man connect
 * It is possible to select(2) or poll(2) for completion by selecting the socket
 * for writing.  After select(2) indicates  writability,  use getsockopt(2)  to
 * read the SO_ERROR option at level SOL_SOCKET to determine whether connect()
 * completed successfully (SO_ERROR is zero) or unsuccessfully (SO_ERROR is one
 * of  the  usual  error  codes  listed  here,explaining the reason for the failure)
 * 1）连接成功建立时，socket 描述字变为可写。（连接建立时，写缓冲区空闲，所以可写）
 * 2）连接建立失败时，socket 描述字既可读又可写。 （由于有未决的错误，从而可读又可写）
 */
void lsocket::connect_cb ( int32 revents )
{
    assert( "libev connect cb error",!(EV_ERROR & revents) );

    int32 err = _socket.validate();

    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_connection);
    int32 param = 1;
    if ( ref_self )
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref_self);
        param ++;
    }
    lua_pushboolean( L,!err );
    if ( expect_false( LUA_OK != lua_pcall(L,param,0,0) ) )
    {
        ERROR( "connect_cb call lua fail:%s\n",lua_tostring(L,-1) );
        // DON NOT return
    }

    if ( err )  /* 连接失败 */
    {
        _socket.stop();
        ERROR( "socket connect unsuccess:%s\n",strerror(err) ); // not errno
        return;
    }

    KEEP_ALIVE( _socket.fd() );
    USER_TIMEOUT( _socket.fd() );

    _socket.set<lsocket,&lsocket::message_cb>( this );
    _socket.set( EV_READ ); /* 将之前的write改为read */
}
