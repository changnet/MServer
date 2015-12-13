#include "lsocket.h"
#include "ltools.h"
#include "../lua/leventloop.h"
#include "../lua/lclass.h"
#include "../net/buffer_process.h"
#include "../net/packet.h"

lsocket::lsocket( lua_State *L )
    : L(L)
{
    ref_self       = 0;
    ref_read       = 0;
    ref_accept     = 0;
    ref_connected  = 0;
    ref_disconnect = 0;
}

lsocket::~lsocket()
{
    socket::close(); /* lsocket的内存由lua控制，保证在释放socket时一定会关闭 */

    /* 释放引用，如果有内存问题，可查一下这个地方 */
    LUA_UNREF( ref_self       );
    LUA_UNREF( ref_read       );
    LUA_UNREF( ref_accept     );
    LUA_UNREF( ref_connected  );
    LUA_UNREF( ref_disconnect );

    assert( "socket not clean",0 == sending && (!w.is_active()) );
}

/* 发送数据 */
int32 lsocket::send()
{
    if ( !w.is_active() )
    {
        return luaL_error( L,"socket not valid" );
    }

    /* TODO
     * 发送数据要先把数据放到缓冲区，然后加入send列表，最后在epoll循环中发送，等待
     * ev整合到backend中才处理
     * 现在只是处理字符串
     */
     return 0;
}

int32 lsocket::kill()
{
    if ( !w.is_active() ) return 0;

    if ( _send.data_size() > 0 ) /* 尝试把缓冲区的数据直接发送 */
    {
        _send.send( w.fd );
    }

    socket::close();

    return 0;
}

int32 lsocket::listen()
{
    if ( w.is_active() )
    {
        return luaL_error( L,"listen:socket already active");
    }

    const char *addr = luaL_checkstring( L,1 );
    if ( !addr )
    {
        return luaL_error( L,"listen:address not specify" );
    }

    int32 port = luaL_checkinteger( L,2 );
    _type = static_cast<socket::SOCKET_TYPE>( luaL_checkinteger( L, 3 ) );

    int32 fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if ( fd < 0 )
    {
        lua_pushnil( L );
        return 1;
    }

    int32 optval = 1;
    /*
     * enable address reuse.it will help when the socket is in TIME_WAIT status.
     * for example:
     *     server crash down and the socket is still in TIME_WAIT status.if try
     * to restart server immediately,you need to reuse address.but note you may
     * receive the old data from last time.
     */
    if ( setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,(char *) &optval, sizeof(optval)) < 0 )
    {
        ::close( fd );
        lua_pushnil( L );
        return 1;
    }

    if ( socket::non_block( fd ) < 0 )
    {
        ::close( fd );
        lua_pushnil( L );
        return 1;
    }

    struct sockaddr_in sk_socket;
    memset( &sk_socket,0,sizeof(sk_socket) );
    sk_socket.sin_family = AF_INET;
    sk_socket.sin_addr.s_addr = inet_addr(addr);
    sk_socket.sin_port = htons( port );

    if ( ::bind( fd, (struct sockaddr *) & sk_socket,sizeof(sk_socket)) < 0 )
    {
        ::close( fd );

        lua_pushnil( L );
        return 1;
    }

    if ( ::listen( fd, 256 ) < 0 )
    {
        ::close( fd );
        lua_pushnil( L );
        return 1;
    }

    class ev_loop *loop = static_cast<class ev_loop *>( leventloop::instance() );
    w.set( loop );
    w.set<lsocket,&lsocket::listen_cb>( this );
    w.start( fd,EV_READ );

    lua_pushinteger( L,fd );
    return 1;
}

int32 lsocket::connect()
{
    if ( w.is_active() )
    {
        return luaL_error( L,"connect:socket already active");
    }

    const char *addr = luaL_checkstring( L,1 );
    if ( !addr )
    {
        return luaL_error( L,"connect:address not specify" );
    }

    const int32 port = luaL_checkinteger( L,2 );
    _type = static_cast<socket::SOCKET_TYPE>( luaL_checkinteger( L,3 ) );

    int32 fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if ( fd < 0 )
    {
        lua_pushnil( L );
        return 1;
    }


    if ( socket::non_block( fd ) < 0 )
    {
        lua_pushnil( L );
        return 1;
    }

    struct sockaddr_in sk_socket;
    memset( &sk_socket,0,sizeof(sk_socket) );
    sk_socket.sin_family = AF_INET;
    sk_socket.sin_addr.s_addr = inet_addr(addr);
    sk_socket.sin_port = htons( port );

    /* 三次握手是需要一些时间的，内核中对connect的超时限制是75秒 */
    if ( ::connect( fd, (struct sockaddr *) & sk_socket,sizeof(sk_socket)) < 0
        && errno != EINPROGRESS )
    {
        ::close( fd );

        lua_pushnil( L );
        return 1;
    }

    class ev_loop *loop = static_cast<class ev_loop *>( leventloop::instance() );
    w.set( loop );
    w.set<lsocket,&lsocket::connect_cb>( this );
    w.start( fd,EV_WRITE ); /* write事件 */

    lua_pushinteger( L,fd );
    return 1;
}

/* 发送原始数据，二进制或字符串 */
int32 lsocket::raw_send()
{
    if ( !w.is_active() )
    {
        return luaL_error( L,"raw_send:socket not valid");
    }

    /* 不要用luaL_checklstring，它给出的长度不包含字符串结束符\0，而我们不知道lua发送的
     * 是字符串还是二进制，因此在lua层要传入一个长度，默认为字符串，由optional取值
     */
    const char *sz = luaL_checkstring( L,1 );
    const int32 len = luaL_optinteger( L,2,strlen(sz)+1 );

    if ( !sz || len <= 0 )
    {
        return luaL_error( L,"raw_send nothing to send" );
    }

    assert( "raw_send illegal fd",w.fd > 0 );

    /* raw_send是一个原始的发送函数,数据不经过协议处理(不经protobuf
     * 处理，不包含协议号...)，可以发送二进制或字符串。
     * 比如把战报写到文件，读出来后可以直接用此函数发送
     * 又或者向php发送json字符串
     */
    this->_send.append( sz,len );
    leventloop::instance()->pending_send( static_cast<class socket*>(this) );  /* 放到发送队列，最后一次发送 */

    return 0;
}

void lsocket::listen_cb( ev_io &w,int32 revents )
{
    assert( "libev listen cb  error",!(EV_ERROR & revents) );

    class ev_loop *loop = static_cast<class ev_loop *>( leventloop::instance() );
    while ( w.is_active() )
    {
        int32 new_fd = ::accept( w.fd,NULL,NULL );
        if ( new_fd < 0 )
        {
            if ( EAGAIN != errno && EWOULDBLOCK != errno )
            {
                FATAL( "accept fail:%s\n",strerror(errno) );
                return;
            }

            break;  /* 所有等待的连接已处理完 */
        }

        socket::non_block( new_fd );
        KEEP_ALIVE( new_fd );
        USER_TIMEOUT( new_fd );

        /* 直接进入监听 */
        class lsocket *_socket = new class lsocket( L );
        _socket->set_type( _type );
        (_socket->w).set( loop );
        (_socket->w).set<lsocket,&lsocket::read_cb>( _socket );
        (_socket->w).start( new_fd,EV_READ );  /* 这里会设置fd */

        lua_rawgeti(L, LUA_REGISTRYINDEX, ref_accept);
        int32 param = 1;
        if ( ref_self )
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref_self);
            param ++;
        }
        lclass<lsocket>::push( L,_socket,true );
        if ( expect_false( LUA_OK != lua_pcall(L,param,0,0) ) )
        {
            /* 如果失败，会出现几种情况：
             * 1) lua能够引用socket，只是lua其他逻辑出错
             * 2) lua不能引用socket，导致lua层gc时会销毁socket,ev还来不及fd_reify，又
             *    将此fd删除，触发一个错误
             */
            ERROR( "listen cb call accept handler fail:%s\n",lua_tostring(L,-1) );
            return;
        }
    }
}

void lsocket::read_cb( ev_io &w,int32 revents )
{
    assert( "libev read cb error",!(EV_ERROR & revents) );

    int32 fd = w.fd;

    /* 就游戏中的绝大多数消息而言，一次recv就能接收完成，不需要while接收直到出错。而且
     * 当前设定的缓冲区与socket一致(8192)，socket缓冲区几乎不可能还有数据，不需要多调用
     * 一次recv。退一步，假如还有数据，epoll当前为LT模式，下一次回调再次读取
     * 如果启用while,需要检测_socket在lua层逻辑中是否被关闭
     */

    int32 ret = _recv.recv( fd );

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
            ERROR( "socket read_cb error:%s\n",strerror(errno) );
            on_disconnect();
        }
        return;
    }

    /* read data */
    packet_parse();
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
void lsocket::connect_cb( ev_io &w,int32 revents )
{
    assert( "libev connect cb error",!(EV_ERROR & revents) );

    int32 fd = w.fd;

    int32 error   = 0;
    socklen_t len = sizeof (error);
    if ( getsockopt( fd, SOL_SOCKET, SO_ERROR, &error, &len ) < 0 )
    {
        ERROR( "connect cb getsockopt error:%s\n",strerror(errno) );

        error = errno;
        // DON NOT return
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_connected);
    int32 param = 1;
    if ( ref_self )
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref_self);
        param ++;
    }
    lua_pushboolean( L,!error );
    if ( expect_false( LUA_OK != lua_pcall(L,param,0,0) ) )
    {
        ERROR( "connect_cb call lua fail:%s\n",lua_tostring(L,-1) );
        // DON NOT return
    }

    if ( error )  /* 连接失败 */
    {
        socket::close();
        ERROR( "connect unsuccess:%s\n",strerror(error) ); // not errno
        return;
    }

    KEEP_ALIVE( fd );
    USER_TIMEOUT( fd );

    w.stop();   /* 先stop再设置消耗要小一些 */
    w.set<lsocket,&lsocket::read_cb>( this );
    w.set( EV_READ ); /* 将之前的write改为read */
    w.start();
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
    socket::close();
}

void lsocket::packet_parse()
{
    parse_func pft = NULL;
    if ( socket::SK_SERVER == _type )
    {
        pft = buffer_process::server_parse;
    }
    else if ( socket::SK_CLIENT == _type)
    {
        pft = buffer_process::client_parse;
    }
    else
    {
        on_disconnect();
        ERROR( "packet parse unhandle type" );
        return;
    }

    struct packet _packet;

    while ( _recv.data_size() > 0 )
    {
        int32 result = pft( _recv,_packet );

        if ( result > 0 )
        {
            /* TODO 包自动转发，protobuf协议解析 */
            _recv.moveon( result );

            lua_rawgeti(L, LUA_REGISTRYINDEX, ref_read);
            int32 param = 1;
            if ( ref_self )
            {
                lua_rawgeti(L, LUA_REGISTRYINDEX, ref_self);
                param ++;
            }
            lua_pushstring( L,_packet.pkt );
            if ( expect_false( LUA_OK != lua_pcall(L,param,0,0) ) )
            {
                ERROR( "read_cb fail:%s\n",lua_tostring(L,-1) );
                return;
            }
        }
        else
            break;
    }

    assert( "buffer over boundary",_recv.data_size() >= 0 &&
        _recv.size() <= _recv.length() );
    /* 缓冲区为空，尝试清理悬空区 */
    if ( 0 == _recv.data_size() )
        _recv.clear();
}

int32 lsocket::address()
{
    if ( !w.is_active() )
    {
        return luaL_error( L,"listen:socket already active");
    }

    struct sockaddr_in addr;
    memset( &addr, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);

    if ( getpeername( w.fd, (struct sockaddr *)&addr, &len) < 0 )
    {
        return luaL_error( L,"getpeername error: %s",strerror(errno) );
    }

    lua_pushstring( L,inet_ntoa(addr.sin_addr) );
    return 1;
}

int32 lsocket::set_self()
{
    if ( !lua_istable( L,1 ) )
    {
        return luaL_error( L,"set_self,argument illegal.expect table" );
    }

    LUA_REF( ref_self );

    return 0;
}

int32 lsocket::set_read()
{
    if ( !lua_isfunction( L,1 ) )
    {
        return luaL_error( L,"set_read,argument illegal.expect function" );
    }

    LUA_REF( ref_read );

    return 0;
}

int32 lsocket::set_accept()
{
    if ( !lua_isfunction( L,1 ) )
    {
        return luaL_error( L,"set_accept,argument illegal.expect function" );
    }

    LUA_REF( ref_accept );

    return 0;
}

int32 lsocket::set_connected()
{
    if ( !lua_isfunction( L,1 ) )
    {
        return luaL_error( L,"set_connected,argument illegal.expect function" );
    }

    LUA_REF( ref_connected );

    return 0;
}

int32 lsocket::set_disconnected()
{
    if ( !lua_isfunction( L,1 ) )
    {
        return luaL_error( L,"set_disconnected,argument illegal.expect function" );
    }

    LUA_REF( ref_disconnect );

    return 0;
}

int32 lsocket::file_description()
{
    lua_pushinteger( L,w.fd );
    return 1;
}
