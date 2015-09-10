#include "backend.h"

backend::backend()
{
    loop = NULL;
    L    = NULL;
}

void backend::set( ev_loop *loop,lua_State *L )
{
    this->loop = loop;
    this->L    = L;
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
    lua_pushinteger( L,999 );
    return 1;
}

/* 监听端口 */
int32 backend::listen()
{
    std::cout << lua_gettop(L) << std::endl;
    const char *addr = luaL_checkstring( L,1 );
    int32 port = luaL_checkinteger( L,2 );

    int32 fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if ( fd < 0 )
    {
        luaL_error( L,"create socket fail" );
        lua_pushboolean( L,0 );
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
        lua_pushboolean( L,0 );
        return 1;
    }
    
    if ( noblock( fd ) < 0 )
    {
        close( fd );
        luaL_error( L,"set socket noblock fail" );
        lua_pushboolean( L,0 );
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
        lua_pushboolean( L,0 );
        return 1;
    }

    if ( ::listen( fd, 256 ) < 0 )
    {
        close( fd );
        luaL_error( L,"listen fail" );
        
        lua_pushboolean( L,0 );
        return 1;
    }

    lua_pushboolean( L,1 );
    return 1;
}
