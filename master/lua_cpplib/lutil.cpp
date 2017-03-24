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
static int32 md5( lua_State* L )
{
    size_t len;
    MD5_CTX md;
    char buf[33];
    const char* ptr;
    unsigned char dgst[16];
    
    int32 index = 1;
    const char *fmt = "%02x"; // default format to lower
    if ( !lua_isstring( L,1 ) )
    {
        index = 2;

        if ( lua_toboolean( L,1 ) ) fmt = "%02X";
    }

    MD5_Init( &md );
    for ( int32 i = index; i <= lua_gettop( L ); ++i )
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
    for ( int32 i = 0; i < 16; ++i )
    {
        //--%02x即16进制输出，占2个字节
        snprintf( buf + i*2, 3, fmt, dgst[i] );
    }
    lua_pushlstring( L, buf, 32 );

    return 1;
}

static int32 uuid( lua_State *L )
{
    uuid_t u;
    char b[40] = { 0 };

    bool upper = lua_toboolean( L,1 );

    uuid_generate(u);
    upper ? uuid_unparse_upper(u, b) : uuid_unparse_lower(u, b);
    lua_pushstring(L, b);

    return 1;
}

static int32 uuid_short( lua_State *L )
{
    static char bits = 63; /* 11 11111 */
    static const char *digest = 
            "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+_";
    uuid_t u;
    uuid_generate(u);

    char b[40] = { 0 };
    uuid_unparse(u, b);

    /* typedef unsigned char uuid_t[16]
     * a8d99a61-0a06-4c20-b770-ac911432019e
     * 总长度：16*8=128bit
     * 一个16进制字符(0~F)为0000~1111，可以表示4bit。128/4 = 32个字符(减去4个"-")
     * 一个64进制字符(0~_)为00 0000~11 1111,可以表示6bit。128/6 = 22个字符
     */
    char out[23] = { 0 };
    const char *uuid = reinterpret_cast<const char *>( u );
    for ( int32 index = 0;index < 21;index ++ )
    {
        char val = (*(uuid + index * 6)) & bits;
        
        assert( "uuid_short",val >= 0 && val < 64 );
        out[index] = digest[(int)val];
    }

    /* 21*6 = 126,最后一次的时候只有2bit了，不足一个char */
    char val = (*(uuid + 120)) & 0x3; /* 用00 00011来取最后两位的值 */
    assert( "uuid_short",val >= 0 && val < 64 );
    out[22] = digest[(int)val];

    lua_pushstring(L, b);
    lua_pushstring( L, out );
    return 2;
}

static int32 uuid_short_parse( lua_State *L )
{
    static const char digest[] = 
    {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, //  0 ~ 15
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 16 ~ 31
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,-1, // 32 ~ 47
         0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1, // 48 ~ 63
        -1,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50, // 64 ~ 79
        51,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,63, // 80 ~ 95
        -1,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24, // 96 ~ 111
        25,26,27,28,29,30,31,32,33,34,35,-1,-1,-1,-1,-1  // 112 ~ 127
    }

    size_t len = 0;
    const char *str_uuid = lua_tolstring( L,1,&len );

    if ( len != 22 )
    {
        return luaL_error( "invalid uuid short string" );
    }

    /* uuid为128bit
     * base64一个字符表示6bit，需要替换21次，还差128-21*6=2bit
     * 为了方便，我们直接使用char(8bit)来赋值，需要21*6 + 8 = 134bit
     */
    uuid_t u;
    char *uuid = reinterpret_cast<char *>( u );

    int ascii = int((*(str_uuid + 126)) & 0x3);
    if ( ascii < 0 || ascii > 127 || digest[ascii] < 0 )
    {
        return luaL_error( "invalid uuid short string" );
    }
    // 128-15*8 = 8,覆盖最后8bit，多出来的6bit在高位，下面会重新覆盖
    *(uuid+15) = digest[ascii];

    for ( int32 index = 20;index >= 0;index -- )
    {
        ascii = (int)*(str_uuid + index);
        if ( ascii < 0 || ascii > 127 || digest[ascii] < 0 )
        {
            return luaL_error( "invalid uuid short string" );
        }

        int pos = index*6 - 2;
        buff[]
    }

    return 1;
}

static const luaL_Reg utillib[] =
{
    {"md5", md5},
    {"uuid",uuid},
    {"timeofday", timeofday},
    {"uuid_short",uuid_short},
    {"gethostbyname", gethost},
    {NULL, NULL}
};

int32 luaopen_util( lua_State *L )
{
  luaL_newlib(L, utillib);
  return 1;
}
