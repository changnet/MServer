#ifndef __LTOOLS_H__
#define __LTOOLS_H__

#include "../global/global.h"
#include <lua.h>

#define LUA_REF(x)                         \
    if ( x )    LUA_UNREF(x);              \
    x = luaL_ref( L,LUA_REGISTRYINDEX )

#define LUA_UNREF(x)                                                 \
    do                                                               \
    {                                                                \
        if ( x > 0 )    {luaL_unref( L,LUA_REGISTRYINDEX,x );x = 0;} \
    }while( 0 )

/* lua 5.3支持int64，在转换为bson时需要区分int32、int64 */
static inline int32 is_32bit(int64 v)
{
    return v >= INT_MIN && v <= INT_MAX;
}

/* 在lua目前的版本中,number >= integer，如果连number都无法支持64,将被强制截断 */
static inline void lua_pushint64( lua_State *L,int64 v )
{
    if ( v <= LUA_MAXINTEGER && v >= LUA_MININTEGER )
        lua_pushinteger( L,static_cast<LUA_INTEGER>(v) );
    else
        lua_pushnumber( L,static_cast<LUA_NUMBER>(v) );
}

#endif /* __LTOOLS_H__ */
