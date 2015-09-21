#include "backend.h"

#define LUA_UNREF(x)                            \
    if ( x > 0 )                                \
        luaL_unref( L,LUA_REGISTRYINDEX,x );

backend::backend()
{
    assert( "you can't create a backend without event loop and lua state",false );
}
backend::backend( ev_loop *loop,lua_State *L )
    : loop(loop),L(L)
{
    net_accept     = 0;
    net_read       = 0;
    net_disconnect = 0;
    net_connected  = 0;
    net_self       = 0;

    anios = NULL;
    aniomax = 0;

    timerlist = NULL;
    timerlistmax = 0;
    timerlistcnt = 0;
}

backend::~backend()
{
    while ( aniomax > 0 )
    {
        --aniomax;
        ANIO anio = anios[aniomax];
        if ( anio )
        {
            anio->close();

            delete anio;
            anios[aniomax] = NULL;
        }
    }

    while ( timerlistcnt > 0 )
    {
        --timerlistcnt;
        // TODO
        // ANTIMER*antimer = timerlist + timerlistcnt;
        // w->close();
        // delete w;
    }
    
    delete []anios;
    anios = NULL;
    aniomax = 0;
    
    delete []timerlist;
    timerlist = NULL;
    timerlistmax = 0;
    timerlistcnt = 0;
    
    /* lua source code 'ref = (int)lua_rawlen(L, t) + 1' make sure ref > 0 */
    LUA_UNREF( net_accept );
    LUA_UNREF( net_read );
    LUA_UNREF( net_disconnect );
    LUA_UNREF( net_connected );
    LUA_UNREF( net_self );
}

int32 backend::run()
{
    loop->run();
    return 0;
}

int32 backend::quit()
{
    loop->quit();
    return 0;
}

/* 获取当前时间戳 */
int32 backend::now()
{
    lua_pushinteger( L,loop->now() );
    return 1;
}

/* 监听端口 */
int32 backend::listen()
{
    const char *addr = luaL_checkstring( L,1 );
    int32 port = luaL_checkinteger( L,2 );

    int32 fd = ::socket(AF_INET, SOCK_STREAM, 0);
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
        close( fd );
        luaL_error( L,"setsockopt SO_REUSEADDR fail" );
        lua_pushnil(L);
        return 1;
    }
    
    if ( noblock( fd ) < 0 )
    {
        close( fd );
        luaL_error( L,"set socket noblock fail" );
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
        close( fd );

        luaL_error( L,"bind socket fail" );
        lua_pushnil(L);
        return 1;
    }

    if ( ::listen( fd, 256 ) < 0 )
    {
        close( fd );
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
int32 backend::io_kill()
{
    int32 fd = luaL_checkinteger( L,1 );
    if ( (fd > aniomax - 1) || !anios[fd] )
    {
        luaL_error( L,"io_kill got illegal fd" );
        return 0;
    }

    class socket *s = anios[fd];
    anios[fd] = NULL;
    s->close();

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

        assert( "accept,dirty ANIO detected!!\n",!(anios[new_fd]) );

        lua_rawgeti(L, LUA_REGISTRYINDEX, net_accept);
        lua_rawgeti(L, LUA_REGISTRYINDEX, net_self);
        lua_pushinteger( L,w.fd );
        lua_pushinteger( L,new_fd );
        if ( expect_false( LUA_OK != lua_pcall(L,3,0,0) ) )
        {
            ::close( new_fd );
            ERROR( "listen_cb fail:%s\n",lua_tostring(L,-1) );
            return;
        }

        noblock( new_fd );
        /* 直接进入监听 */
        class socket *_socket = new class socket();
        (_socket->w).set( loop );
        (_socket->w).set<backend,&backend::read_cb>( this );
        (_socket->w).start( new_fd,EV_READ );
        
        array_resize( ANIO,anios,aniomax,new_fd + 1,array_zero );
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
 */
void backend::connect_cb( ev_io &w,int32 revents )
{

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
    while ( true )
    {
        char buff[256];  // TODO
        int32 ret = read( fd,buff,256 );

        if ( ret < 0 )  /* error */
        {
            if ( EAGAIN == errno || EWOULDBLOCK == errno )
                break;

            anios[fd]->close();
            delete anios[fd];
            anios[fd] = NULL;

            lua_rawgeti(L, LUA_REGISTRYINDEX, net_disconnect);
            lua_rawgeti(L, LUA_REGISTRYINDEX, net_self);
            lua_pushinteger( L,fd );
            if ( expect_false( LUA_OK != lua_pcall(L,2,0,0) ) )
            {
                ERROR( "read_cb fail:%s\n",lua_tostring(L,-1) );
                return;
            }
            break;
        }
        else if ( 0 == ret ) /* disconnect */
        {
            anios[fd]->close();
            delete anios[fd];
            anios[fd] = NULL;
            
            lua_rawgeti(L, LUA_REGISTRYINDEX, net_disconnect);
            lua_rawgeti(L, LUA_REGISTRYINDEX, net_self);
            lua_pushinteger( L,fd );
            if ( expect_false( LUA_OK != lua_pcall(L,2,0,0) ) )
            {
                ERROR( "read_cb fail:%s\n",lua_tostring(L,-1) );
                return;
            }
            break;
        }
        else    /* read data */
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, net_read);
            lua_rawgeti(L, LUA_REGISTRYINDEX, net_self);
            lua_pushinteger( L,fd );
            lua_pushstring( L,buff );
            if ( expect_false( LUA_OK != lua_pcall(L,3,0,0) ) )
            {
                ERROR( "read_cb fail:%s\n",lua_tostring(L,-1) );
                return;
            }
        }
    }
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
