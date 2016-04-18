#include <lua.hpp>
#include <sys/time.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "lutil.h"

/* clock_gettime(CLOCK_MONOTONIC)是内核调用，并且不受time jump影响
 * gettimeofday是用户空间调用
 */
static int32 timeofday( lua_State *L )
{
    struct timeval tv;
    if ( gettimeofday( &tv, NULL ) == 0 )
    {
        lua_pushnumber( L, tv.tv_sec );
        lua_pushnumber( L, tv.tv_usec );
        return 2;
    }

    return 0;
}

static int32 gethost( lua_State *L )
{
    const char *name = luaL_checkstring( L,1 );
    if ( !name ) return luaL_error( L,"gethost illegal argument" );

    struct hostent *hptr;

    if( ( hptr = gethostbyname( name ) ) == NULL )
    {
        ERROR( "gethostbyname(%s):%s",name,hstrerror(h_errno) );
        return 0;
    }

    switch( hptr->h_addrtype )
    {
        case AF_INET:
        case AF_INET6:
        {
            int32 return_value = 0;
            char dst[INET6_ADDRSTRLEN];
            char **pptr = hptr->h_addr_list;
            for( ;*pptr != NULL; pptr++ )
            {
                const char *host = inet_ntop( hptr->h_addrtype, *pptr, dst,
                    sizeof(dst) );
                if ( !host )
                {
                    ERROR( "gethostbyname inet_ntop error(%d):%s",errno,
                        strerror(errno) );
                    return return_value;
                }

                lua_pushstring( L,host );
                ++return_value;
            }
            return return_value;
        }break;
        default:
            return luaL_error( L,"gethostbyname unknow address type" );
            break;
    }

    return 0;
}

static const luaL_Reg utillib[] =
{
    {"timeofday", timeofday},
    {"gethostbyname", gethost},
    {NULL, NULL}
};

int32 luaopen_util( lua_State *L )
{
  luaL_newlib(L, utillib);
  return 1;
}
