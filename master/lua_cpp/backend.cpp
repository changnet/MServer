#include "backend.h"
#include "../ev/ev_def.h"
#include "../net/buffer_process.h"
#include "../net/packet.h"

#define LUA_UNREF(x)                            \
    if ( x > 0 )                                \
        luaL_unref( L,LUA_REGISTRYINDEX,x );

#ifdef TCP_KEEP_ALIVE
# define KEEP_ALIVE(x)    socket::keep_alive(x)
#else
# define KEEP_ALIVE(x)
#endif

#ifdef _TCP_USER_TIMEOUT
# define USER_TIMEOUT(x)    socket::user_timeout(x)
#else
# define USER_TIMEOUT(x)
#endif

backend::backend()
{
    assert( "you can't create a backend without event loop and lua state",false );
}

backend::backend( lua_State *L )
    : L(L)
{
    loop = static_cast<class ev_loop *>( this );

    net_accept     = 0;
    net_read       = 0;
    net_disconnect = 0;
    net_connected  = 0;
    net_self       = 0;

    anios = NULL;
    aniomax =  0;
    
    ansendings = NULL;
    ansendingmax =  0;
    ansendingcnt =  0;

    antimers = NULL;
    antimermax =  0;
    antimercnt =  0;
}

backend::~backend()
{
    while ( aniomax > 0 )
    {
        --aniomax;
        ANIO anio = anios[aniomax];
        if ( anio )
        {
            delete anio;
            anios[aniomax] = NULL;
        }
    }

    while ( antimercnt > 0 )
    {
        --antimercnt;
        // TODO

    }
    
    if ( anios )
        delete []anios;
    anios = NULL;
    aniomax = 0;
    
    if ( ansendings )
        delete []ansendings;
    ansendings = NULL;
    ansendingcnt =  0;
    ansendingmax =  0;
    
    if ( antimers )
        delete []antimers;
    antimers = NULL;
    antimermax = 0;
    antimercnt = 0;
    
    /* lua source code 'ref = (int)lua_rawlen(L, t) + 1' make sure ref > 0 */
    LUA_UNREF( net_accept );
    LUA_UNREF( net_read );
    LUA_UNREF( net_disconnect );
    LUA_UNREF( net_connected );
    LUA_UNREF( net_self );
}

/* 因为要加入lua gc等，必须重写基类事件循环函数 */
int32 backend::run()
{
    assert( "backend uninit",backend_fd >= 0 );

    loop_done = false;
    while ( !loop_done )
    {
        fd_reify();/* update fd-related kernel structures */
        
        /* calculate blocking time */
        {
            ev_tstamp waittime  = 0.;
            
            
            /* update time to cancel out callback processing overhead */
            time_update ();
            
            waittime = MAX_BLOCKTIME;
            
            if (timercnt) /* 如果有定时器，睡眠时间不超过定时器触发时间，以免睡过头 */
            {
               ev_tstamp to = (timers [HEAP0])->at - mn_now;
               if (waittime > to) waittime = to;
            }
    
            /* at this point, we NEED to wait, so we have to ensure */
            /* to pass a minimum nonzero value to the backend */
            if (expect_false (waittime < backend_mintime))
                waittime = backend_mintime;

            backend_poll ( waittime );

            /* update ev_rt_now, do magic */
            time_update ();
        }

        /* queue pending timers and reschedule them */
        timers_reify (); /* relative timers called last */
  
        invoke_pending ();
        invoke_sending ();
    }    /* while */
    
    return 0;
}

/* 退出循环 */
int32 backend::exit()
{
    loop->quit();
    return 0;
}

/* 获取当前时间戳 */
int32 backend::time()
{
    lua_pushinteger( L,ev_rt_now );
    return 1;
}

/* 监听端口 */
int32 backend::listen()
{
    const char *addr = luaL_checkstring( L,1 );
    int32 port = luaL_checkinteger( L,2 );

    int32 fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if ( fd < 0 )
    {
        luaL_error( L,"create socket fail" );
        lua_pushnil(L);
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
        luaL_error( L,"setsockopt SO_REUSEADDR fail" );
        lua_pushnil(L);
        return 1;
    }
    
    if ( socket::non_block( fd ) < 0 )
    {
        ::close( fd );
        luaL_error( L,"listen set socket non-block fail" );
        lua_pushnil(L);
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

        luaL_error( L,"bind socket fail" );
        lua_pushnil(L);
        return 1;
    }

    if ( ::listen( fd, 256 ) < 0 )
    {
        ::close( fd );
        luaL_error( L,"listen fail" );
        
        lua_pushnil(L);
        return 1;
    }

    class socket *_socket = new class socket();
    (_socket->w).set( loop );
    (_socket->w).set<backend,&backend::listen_cb>( this );
    (_socket->w).start( fd,EV_READ );

    array_resize( ANIO,anios,aniomax,fd + 1,array_zero );
    
    assert( "listen,dirty ANIO detected!!\n",!(anios[fd]) );
    anios[fd] = _socket;

    lua_pushinteger( L,fd );
    return 1;
}

/* 从lua层停止一个io watcher */
/* TODO 这里应该是io_death，然后放到一个changne list待处理 */
int32 backend::io_kill()
{
    int32 fd = luaL_checkinteger( L,1 );
    if ( fd < 0 || (fd > aniomax - 1) || !anios[fd] )
    {
        luaL_error( L,"io_kill got illegal fd" );
        return 0;
    }

    delete anios[fd];
    anios[fd] = NULL;

    return 0;
}

/* 监听回调 */
void backend::listen_cb( ev_io &w,int revents )
{
    if ( EV_ERROR & revents )
    {
        FATAL( "listen_cb libev error,abort .. \n" );
        return;
    }

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

        lua_rawgeti(L, LUA_REGISTRYINDEX, net_accept);
        lua_rawgeti(L, LUA_REGISTRYINDEX, net_self);
        lua_pushinteger( L,w.fd );
        lua_pushinteger( L,new_fd );
        if ( expect_false( LUA_OK != lua_pcall(L,3,1,0) ) )
        {
            ::close( new_fd );
            ERROR( "listen cb call accept handler fail:%s\n",lua_tostring(L,-1) );
            return;
        }
        
        if ( !lua_isinteger(L, -1) )
        {
            ::close( new_fd );
            ERROR( "function `on_accept' must return a socket type,got %s\n",
                lua_typename(L, lua_type(L, -1)) );
            lua_pop( L,1 );
            return;
        }

        socket::SOCKET_TYPE sk = static_cast<socket::SOCKET_TYPE>
            ( lua_tonumber(L, -1) );
        lua_pop(L, -1);  /* pop returned value */
        if ( socket::SK_ERROR == sk )
        {
            ERROR( "lua accept socket return socket error" );
            ::close( new_fd );
            return;
        }
        
        socket::non_block( new_fd );
        KEEP_ALIVE( new_fd );
        USER_TIMEOUT( new_fd );
        /* 直接进入监听 */
        class socket *_socket = new class socket();
        _socket->set_type( sk );
        (_socket->w).set( loop );
        (_socket->w).set<backend,&backend::read_cb>( this );
        (_socket->w).start( new_fd,EV_READ );
        
        array_resize( ANIO,anios,aniomax,new_fd + 1,array_zero );
        assert( "accept,dirty ANIO detected!!\n",!(anios[new_fd]) );
        anios[new_fd] = _socket;
    }
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
void backend::connect_cb( ev_io &w,int32 revents )
{
    if ( EV_ERROR & revents )
    {
        FATAL( "connect_cb libev error,abort .. \n" );
        return;
    }
    
    int32 fd = w.fd;
    
    int32 error   = 0;
    socklen_t len = sizeof (error);
    if ( getsockopt( fd, SOL_SOCKET, SO_ERROR, &error, &len ) < 0 )
    {
        ERROR( "connect cb getsockopt error:%s\n",strerror(errno) );
        
        delete anios[fd];
        anios[fd] = NULL;
        
        return;
    }
    
    lua_rawgeti(L, LUA_REGISTRYINDEX, net_connected);
    lua_rawgeti(L, LUA_REGISTRYINDEX, net_self);
    lua_pushinteger( L,fd );
    lua_pushboolean( L,!error );
    if ( expect_false( LUA_OK != lua_pcall(L,3,0,0) ) )
    {
        ERROR( "read_cb fail:%s\n",lua_tostring(L,-1) );
        // DON NOT return;
    }

    if ( error )  /* 连接失败 */
    {
        delete anios[fd];
        anios[fd] = NULL;

        ERROR( "connect unsuccess:%s\n",strerror(error) );
        return;
    }

    KEEP_ALIVE( fd );
    USER_TIMEOUT( fd );

    class socket *_socket = anios[fd];
    (_socket->w).stop();   /* 先stop再设置消耗要小一些 */
    (_socket->w).set<backend,&backend::read_cb>( this );
    (_socket->w).set( EV_READ ); /* 将之前的write改为read */
    (_socket->w).start();
}

/* 读回调 */
void backend::read_cb( ev_io &w,int revents )
{
    if ( EV_ERROR & revents )
    {
        FATAL( "listen_cb libev error,abort .. \n" );
        return;
    }
    
    int32 fd = w.fd;
    /* TODO _socket是否被关闭 */
    while ( true )
    {
        class socket *_socket = anios[fd];
        int32 ret = _socket->_recv.recv( fd );

        /* disconnect or error */
        if ( 0 == ret )
        {
            socket_disconect( fd );
            break;
        }
        else if ( ret < 0 )
        {
            if ( EAGAIN != errno && EWOULDBLOCK != errno )
                socket_disconect( fd );
            break;
        }
  
        /* read data */
        packet_parse( fd,_socket );
    }
}

/* 网络包解析 */
void backend::packet_parse( int32 fd,class socket *_socket )
{
    parse_func pft = NULL;
    if ( socket::SK_SERVER == _socket->_type )
    {
        pft = buffer_process::server_parse;
    }
    else
    {
        pft = buffer_process::client_parse;
    }
    
    struct packet _packet;
    
    /* TODO 这里可能会出现在一个逻辑里面把当前socket给关闭甚至delete掉
     * 因此这里需要同时检测当前socket是否还存活
     */
    while ( _socket->_recv.data_size() > 0 )
    {
        int32 result = pft( _socket->_recv,_packet );
        if ( result > 0 )
        {
            /* TODO 包自动转发，protobuf协议解析 */
            _socket->_recv.moveon( result );

            lua_rawgeti(L, LUA_REGISTRYINDEX, net_read);
            lua_rawgeti(L, LUA_REGISTRYINDEX, net_self);
            lua_pushinteger( L,fd );
            lua_pushstring( L,_packet.pkt );
            if ( expect_false( LUA_OK != lua_pcall(L,3,0,0) ) )
            {
                ERROR( "read_cb fail:%s\n",lua_tostring(L,-1) );
                return;
            }
        }
        else
            break;
    }
    
    assert( "buffer over boundary",_socket->_recv.data_size() >= 0 &&
        _socket->_recv.size() < _socket->_recv.length() );
    /* 缓冲区为空，尝试清理悬空区 */
    if ( 0 == _socket->_recv.data_size() )
        _socket->_recv.clear();
}

/* 设置lua层网络事件回调 */
int32 backend::set_net_ref()
{
    if ( !lua_istable( L,1 ) || !lua_isfunction( L,2 ) ||
        !lua_isfunction( L,3 ) || !lua_isfunction( L,4 ) ||
        !lua_isfunction( L,5 ) )
    {
        luaL_error( L,"set_net_ref,argument illegal.expect table and function\n" );
        return 0;
    }

    net_connected  = luaL_ref( L,LUA_REGISTRYINDEX );
    net_disconnect = luaL_ref( L,LUA_REGISTRYINDEX );
    net_read       = luaL_ref( L,LUA_REGISTRYINDEX );
    net_accept     = luaL_ref( L,LUA_REGISTRYINDEX );
    net_self       = luaL_ref( L,LUA_REGISTRYINDEX );

    return 0;
}

/* 获取对方ip */
int32 backend::fd_address()
{
    int32 fd = luaL_checkinteger( L,1 );
    
    struct sockaddr_in addr;
    memset( &addr, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);
    
    if ( getpeername(fd, (struct sockaddr *)&addr, &len) < 0 )
    {
        luaL_error( L,"getpeername error: %s",strerror(errno) );
         return 0;
    }
    
    lua_pushstring( L,inet_ntoa(addr.sin_addr) );
    return 1;
}

/* 主动tcp连接 ev:connect( "127.0.0.1",9999,ev.SK_SERVER )*/
int32 backend::connect()
{
    const char *addr = luaL_checkstring( L,1 );
    const int32 port = luaL_checkinteger( L,2 );
    const socket::SOCKET_TYPE sk = static_cast<socket::SOCKET_TYPE>(
        luaL_optinteger( L,3,socket::SK_SERVER ) );

    int32 fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if ( fd < 0 )
    {
        luaL_error( L,"create socket fail" );
        lua_pushnil(L);
        return 1;
    }


    if ( socket::non_block( fd ) < 0 )
    {
        ::close( fd );
        luaL_error( L,"connect set socket non-block fail" );
        lua_pushnil(L);
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

        luaL_error( L,"bind socket fail" );
        lua_pushnil(L);
        return 1;
    }
    
    class socket *_socket = new class socket();
    _socket->set_type( sk );
    (_socket->w).set( loop );
    (_socket->w).set<backend,&backend::connect_cb>( this );
    (_socket->w).start( fd,EV_WRITE ); /* write事件 */

    array_resize( ANIO,anios,aniomax,fd + 1,array_zero );

    assert( "connect,dirty ANIO detected!!\n",!(anios[fd]) );
    anios[fd] = _socket;

    lua_pushinteger( L,fd );
    return 1;
}

/* 发送数据 */
int32 backend::send()
{
    /* TODO
     * 发送数据要先把数据放到缓冲区，然后加入send列表，最后在epoll循环中发送，等待
     * ev整合到backend中才处理
     * 现在只是处理字符串
     */
     return 0;
}

/* 发送原始数据，二进制或字符串 */
int32 backend::raw_send()
{
    int32 fd = luaL_checkinteger( L,1 );
    
    /* 不要用luaL_checklstring，它给出的长度不包含字符串结束符\0，而我们不知道lua发送的
     * 是字符串还是二进制，因此在lua层要传入一个长度，默认为字符串，由optional取值
     */
    const char *sz = luaL_checkstring( L,2 );
    const int32 len = luaL_optinteger( L,3,strlen(sz)+1 );

    if ( !sz || len <= 0 )
    {
        luaL_error( L,"raw_send nothing to send" );
        return 0;
    }
    
    if ( fd < 0 || fd >= aniomax || !anios[fd] )
    {
        luaL_error( L,"raw_send illegal fd" );
        return 0;
    }
    
    class socket *_socket = anios[fd];

    /* raw_send是一个原始的发送函数,数据不经过协议处理(不经protobuf
     * 处理，不包含协议号...)，可以发送二进制或字符串。
     * 比如把战报写到文件，读出来后可以直接用此函数发送
     * 又或者向php发送json字符串
     */
    _socket->_send.append( sz,len );
    slist_add( fd,_socket );  /* 放到发送队列，最后一次发送 */
    
    return 0;
}

/* 加入到发送列表 */
void backend::slist_add( int32 fd,class socket *_socket )
{
    if ( _socket->sending )  /* 已经标记为发送，无需再标记 */
        return;

    // 0位是空的，不使用
    ++ansendingcnt;
    array_resize( ANSENDING,ansendings,ansendingmax,ansendingcnt + 1,EMPTY );
    ansendings[ansendingcnt] = fd;
    
    _socket->sending = ansendingcnt;
}

/* 把数据攒到一起，一次发送
 * 发处是：把包整合，减少发送次，提高效率
 * 坏处是：如果发送的数据量很大，在逻辑处理过程中就不能利用带宽
 * 然而，游戏中包多，但数据量不大
 */
void backend::invoke_sending()
{
    if ( ansendingcnt <= 0 )
        return;

    int32 fd   = 0;
    int32 empty = 0;
    int32 empty_max = 0;
    class socket *_socket = NULL;
    
    /* 这里使用一个数组来管理要发送数据的socket。还有一种更简单粗暴的方法是每次都遍历所有
     * socket，如果有数据在缓冲区则发送，这种方法在多链接，低活跃场景中效率不高。
     */
    for ( int32 i = 1;i <= ansendingcnt;i ++ )/* 0位是空的，不使用 */
    {
        if ( !(fd = ansendings[i]) || !(_socket = anios[fd]) )
        {
            /* 假如socket发送了很大的数据并直接关闭socket。一次event loop无法发送完数据，
             * socket会被强制关闭。此时就会出现anios[fd] = NULL。考虑到这种情况在游戏中
             * 并不常见，可当异常处理
             */
            ERROR( "invoke sending empty socket" );
            ++empty;
            empty_max = i;
            continue;
        }
        
        assert( "invoke sending index not match",i == _socket->sending );

        /* 处理发送 */
        int32 ret = _socket->_send.send( fd );
        if ( 0 == ret || (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK) )
        {
            ERROR( "invoke sending unsuccess:%s\n",strerror(errno) );
            socket_disconect( fd );
            ++empty;
            empty_max = i;
            continue;
        }
        
        /* 处理sendings移动 */
        if ( _socket->_send.data_size() <= 0 )
        {
            _socket->sending = 0;   /* 去除发送标识 */
            _socket->_send.clear(); /* 去除悬空区 */
            ++empty;
            empty_max = i;
        }
        else if ( empty )  /* 将发送数组向前移动，防止中间留空 */
        {
            int32 empty_min = empty_max - empty + 1;
            _socket->sending = empty_min;
            --empty;
        }
        /* 数据未发送完，也不需要移动，则do nothing */
    }

    ansendingcnt -= empty;
    assert( "invoke sending sending counter fail",ansendingcnt >= 0 && ansendingcnt < ansendingmax );
}

/* socket 断开事件，通知lua层 */
void backend::socket_disconect( int32 fd )
{
    delete anios[fd];
    anios[fd] = NULL;

    lua_rawgeti(L, LUA_REGISTRYINDEX, net_disconnect);
    lua_rawgeti(L, LUA_REGISTRYINDEX, net_self);
    lua_pushinteger( L,fd );
    if ( expect_false( LUA_OK != lua_pcall(L,2,0,0) ) )
    {
        ERROR( "socket disconect,call lua fail:%s\n",lua_tostring(L,-1) );
    }
}
