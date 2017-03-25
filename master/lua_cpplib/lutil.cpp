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
    static const char *digest = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+_";
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
    char val;
    char fragment;
    char out[23] = { 0 };
    const char *uuid = reinterpret_cast<const char *>( u );

    /* 3*8 = 4*6 = 24,128/24 = 5
     * 64bit表示6bit，一个char为8bit，每次编码3个char，产生4个符。5次共编码120bit
     */
    char *cur_char = out;
    for ( int32 index = 0;index < 5;index ++ )
    {

        /* 1111 1100取高6bit，再右移2bit得到前6bit */
        /* 0000 0011取低2bit，左移4bit存到val的2~3bit */
        fragment = *uuid ++;
        val  = (fragment & 0x0fc) >> 2;
        *cur_char++ = digest[(int)val];
        val  = (fragment & 0x003) << 4;

        /* 1111 0000取高4bit，右移4bit得到低4bit，加上上一步val的高2bit，得6bit */
        /* 0000 1111取低4bit，左移2bit暂存到val的2~5bit */
        fragment = *uuid ++;
        val |= (fragment & 0x0f0) >> 4;
        *cur_char++ = digest[(int)val];
        val  = (fragment & 0x00f) << 2;

        /* 1100 0000先取高2bit，右移6bit，加上上一步val的高4bit,得6bit */
        fragment = *uuid ++;
        val |= (fragment & 0x0c0) >> 6;
        *cur_char++ = digest[(int)val];
        val  = (fragment & 0x03f) << 0;

        /* 0011 1111 完整的低6bit */
        *cur_char++ = digest[(int)val];
    }

    // 余下的8bit，特殊处理
    fragment = *uuid ++;
    val  = (fragment & 0x0fc) >> 2;
    *cur_char++ = digest[(int)val];

    /* 最后一个只有2bit */
    val  = (fragment & 0x003);
    *cur_char = digest[(int)val];

    lua_pushstring( L, b   );
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
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1, // 48 ~ 63
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,23,14, // 64 ~ 79
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,63, // 80 ~ 95
        -1,26,17,28,29,30,31,32,33,34,35,36,37,38,39,40, // 96 ~ 111
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1  // 112 ~ 127
    };

    size_t len = 0;
    const char *str_uuid = lua_tolstring( L,1,&len );

    if ( len != 22 )
    {
        return luaL_error( L,"invalid uuid short string" );
    }

    uuid_t u;
    char *uuid = reinterpret_cast<char *>( u );

    char fragment;
    /* 每次取4个字符填充到3个char，5次填充20字符,120bit */
    for ( int32 index = 0;index < 5;index ++ )
    {
        fragment = digest[(int)*str_uuid++];
        // 0011 1111 填充高6bit
        *uuid = (fragment & 0x03f) << 2;

        fragment = digest[(int)*str_uuid++];
        // 0011 0000 取6bit中的高2bit，加上一步骤的6bit，填充完一个char
        *uuid++ |= (fragment & 0x030) >> 4;
        // 0000 1111 取剩余下4bit填充到一个新char的高4bit
        *uuid    = (fragment & 0x00f) << 4;

        fragment = digest[(int)*str_uuid++];
        // 0011 1100 取6bit中的高4bit，加上一步骤的4bit，填充完一个char
        *uuid++ |= (fragment & 0x03c) >> 2;
        // 0000 0011 取剩余的2bit，填充到一个新char的高2bit
        *uuid    = (fragment & 0x003) << 6;

        fragment = digest[(int)*str_uuid++];
        // 0011 1111 填充低6bit,加上一步骤的2bit，填充完一个char
        *uuid++ |= (fragment & 0x03f);
    }

    fragment = digest[(int)*str_uuid++];
    // 0011 1111 填充高6bit
    *uuid    = (fragment & 0x03f) << 2;

    fragment = digest[(int)*str_uuid];
    // 0000 0011 取剩余的2bit，填充到一个char的低2bit
    *uuid   |= (fragment & 0x003);

    char b[40] = { 0 };
    uuid_unparse(u, b);
    lua_pushstring(L, b);

    return 1;
}

static const luaL_Reg utillib[] =
{
    {"md5", md5},
    {"uuid",uuid},
    {"timeofday", timeofday},
    {"uuid_short",uuid_short},
    {"gethostbyname", gethost},
    {"uuid_short_parse",uuid_short_parse},
    {NULL, NULL}
};

int32 luaopen_util( lua_State *L )
{
  luaL_newlib(L, utillib);
  return 1;
}
