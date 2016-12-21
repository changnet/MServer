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
    socket::stop(); /* lsocket的内存由lua控制，保证在释放socket时一定会关闭 */

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
    socket::stop();

    return 0;
}

int32 lsocket::listen()
{
    if ( socket::active() )
    {
        return luaL_error( L,"listen:socket already active");
    }

    const char *host = luaL_checkstring( L,1 );
    if ( !host )
    {
        return luaL_error( L,"listen:address not specify" );
    }

    int32 port = luaL_checkinteger( L,2 );
    int32 fd = socket::listen( host,port );
    if ( fd < 0 )
    {
        luaL_error( L,strerror(errno) );
        return 0;
    }

    socket::set<lsocket,&lsocket::listen_cb>( this );
    socket::start( fd,EV_READ );

    lua_pushinteger( L,fd );
    return 1;
}

int32 lsocket::connect()
{
    if ( socket::active() )
    {
        return luaL_error( L,"connect:socket already active");
    }

    const char *host = luaL_checkstring( L,1 );
    if ( !host )
    {
        return luaL_error( L,"connect:host not specify" );
    }

    int32 port = luaL_checkinteger( L,2 );

    int32 fd = socket::connect( host,port );
    if ( fd < 0 )
    {
        luaL_error( L,strerror(errno) );
        return 0;
    }

    socket::set<lsocket,&lsocket::connect_cb>( this );
    socket::start( fd,EV_WRITE ); /* write事件 */

    lua_pushinteger( L,fd );
    return 1;
}

/* 发送原始数据，二进制或字符串 */
int32 lsocket::send()
{
    if ( !socket::active() )
    {
        return luaL_error( L,"socket::send not valid");
    }

    size_t len = 0;
    const char *sz = luaL_checklstring( L,1,&len );
    /* 允许指定发送的长度 */
    len = luaL_optinteger( L,2,len );

    if ( !sz || len <= 0 )
    {
        return luaL_error( L,"socket::send nothing to send" );
    }

    if ( !socket::append( sz,len ) )
    {
        return luaL_error( L,"socket::send out of buffer" );
    }

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

    int32 ret = socket::recv();

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
            ERROR( "socket recv error(maybe out of buffer):%s\n",strerror(errno) );
            on_disconnect();
        }
        return;
    }

    /* 此框架中，socket的内存由lua管理，无法预知lua会何时释放内存
     * 因此不要在C++层用while去解析协议
     */
    int32 complete = is_message_complete();
    if ( complete < 0 )  //error
    {
        on_disconnect();
        return;
    }

    if ( 0 == complete ) return;

    lua_pushcfunction(L,traceback);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_message);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_self);
    if ( expect_false( LUA_OK != lua_pcall(L,1,0,-3) ) )
    {
        ERROR( "message callback fail:%s\n",lua_tostring(L,-1) );

        lua_pop(L,2); /* remove error message and traceback */
        return;
    }

    lua_pop(L,1); /* remove traceback */
}

void lsocket::on_disconnect()
{
    /* close fd first,in case reconnect in lua callback */
    socket::stop();

    lua_pushcfunction(L,traceback);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_disconnect);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_self);
    if ( expect_false( LUA_OK != lua_pcall(L,1,0,-3) ) )
    {
        ERROR( "socket disconect,call lua fail:%s\n",lua_tostring(L,-1) );
        // DO NOT RETURN
        lua_pop(L,1); /* remove error message */
    }

    lua_pop(L,1); /* remove traceback */
}

int32 lsocket::address()
{
    if ( !socket::active() )
    {
        return luaL_error( L,"socket::address not a active socket" );
    }

    lua_pushstring( L,socket::address() );
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
    lua_pushinteger( L,socket::fd() );
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

    int32 err = socket::validate();

    lua_pushcfunction(L,traceback);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_connection);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_self);
    lua_pushboolean( L,!err );
    if ( expect_false( LUA_OK != lua_pcall(L,2,0,-4) ) )
    {
        ERROR( "connect_cb call lua fail:%s\n",lua_tostring(L,-1) );
        lua_pop(L,1); /* remove error message */
        // DON NOT return
    }

    lua_pop(L,1); /* remove traceback */
    if ( err )  /* 连接失败 */
    {
        socket::stop();
        ERROR( "socket connect unsuccess:%s\n",strerror(err) ); // not errno
        return;
    }

    KEEP_ALIVE( socket::fd() );
    USER_TIMEOUT( socket::fd() );

    socket::set<lsocket,&lsocket::message_cb>( this );
    socket::set( EV_READ ); /* 将之前的write改为read */
}

int32 lsocket::buffer_setting()
{
    int min_buff = luaL_checkinteger( L,1 );
    int max_buff = luaL_checkinteger( L,2 );

    if ( min_buff <= 0 || max_buff <= 0 || max_buff <= min_buff )
    {
        return luaL_error( L,"illegal buffer setting" );
    }

    _recv._min_buff = min_buff;
    _recv._max_buff = max_buff;

    _send._min_buff = min_buff;
    _send._max_buff = max_buff;

    return 0;
}


void lsocket::listen_cb( int32 revents )
{
    assert( "libev listen cb  error",!(EV_ERROR & revents) );

    lua_pushcfunction(L,traceback);

    int32 top = lua_gettop(L);
    while ( socket::active() )
    {
        int32 new_fd = socket::accept();
        if ( new_fd < 0 )
        {
            if ( EAGAIN != errno && EWOULDBLOCK != errno )
            {
                FATAL( "http_socket::accept fail:%s\n",strerror(errno) );
                return;
            }

            break;  /* 所有等待的连接已处理完 */
        }

        socket::non_block( new_fd );
        KEEP_ALIVE( new_fd );
        USER_TIMEOUT( new_fd );

        const class lsocket *_s = accept_new( new_fd );

        lua_rawgeti(L, LUA_REGISTRYINDEX, ref_acception);
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref_self);

        _s->push(); /* push socket object to lua stack */
        if ( expect_false( LUA_OK != lua_pcall(L,2,0,top) ) )
        {
            /* 如果失败，会出现几种情况：
             * 1) lua能够引用socket，只是lua其他逻辑出错.需要lua层处理此异常
             * 2) lua不能引用socket，导致lua层gc时会销毁socket,ev还来不及fd_reify，又
             *    将此fd删除，触发一个错误
             */
            lua_remove(L,top); /* remove traceback */
            ERROR( "listen cb call accept handler fail:%s",lua_tostring(L,-1) );
            return;
        }
    }

    lua_remove(L,top); /* remove traceback */
}
