#include <lua.hpp>
#include <sys/time.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <uuid/uuid.h>
#include<openssl/md5.h>

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

/* 域名转Ip,阻塞方式 */
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
                if ( lua_gettop( L ) > 256 )
                {
                    ERROR( "too many ip found,truncate" );
                    return return_value;
                }
                lua_checkstack( L,1 );
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

/* md5
 * md5( [upper,],str1,str2,... )
 */
static int md5( lua_State* L )
{
    size_t len;
    MD5_CTX md;
    char buf[33];
    const char* ptr;
    unsigned char dgst[16];
    
    int index = 1;
    const char *fmt = "%02x"; // default format to lower
    if ( !lua_isstring( L,1 ) )
    {
        index = 2;

        if ( lua_toboolean( L,1 ) ) fmt = "%02X";
    }

    MD5_Init( &md );
    for ( int i = index; i <= lua_gettop( L ); ++i )
    {
        ptr = lua_tolstring( L, i, &len );
        if ( !ptr )
        {
            // not a string and can not convert to string
            return luaL_error( L,
                "argument #%d expect function,got %s",
                i,lua_typename( L,lua_type(L,i) ) );
        }

        MD5_Update( &md, ptr, len );
    }

    MD5_Final( dgst, &md );
    for ( int i = 0; i < 16; ++i )
    {
        //--%02x即16进制输出，占2个字节
        snprintf( buf + i*2, 3, fmt, dgst[i] );
    }
    lua_pushlstring( L, buf, 32 );

    return 1;
}

static int uuid( lua_State* L )
{
    uuid_t u;
    
    char b[40] = { 0 };

    uuid_generate(u);
    uuid_unparse(u, b);
    lua_pushstring(L, b);

    return 1;
}

static const luaL_Reg utillib[] =
{
    {"md5", md5},
    {"uuid",uuid},
    {"timeofday", timeofday},
    {"gethostbyname", gethost},
    {NULL, NULL}
};

int32 luaopen_util( lua_State *L )
{
  luaL_newlib(L, utillib);
  return 1;
}
