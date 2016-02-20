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
static inline int32 lua_isbit32(int64 v)
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

/* 判断一个table是否为数组
 * 1).判断key是否为number并且从1增长
 * 2).由程序员在元表指定__array为true
 * 方法1需要遍历，不太准确(map也有可能以数字为key).但无需要额外指定元表字段.而且bson也
 * 只能以string为key。方法2准确高效，但需要额外指定元表字段
 */
static inline int32 lua_isarray( lua_State *L,int32 index )
{
    if ( !luaL_getmetafield(L, index, "__array") ) return false;

    if ( LUA_TBOOLEAN != lua_type( L,-1) || !lua_toboolean( L,-1 ) )
    {
        lua_pop( L,1 );
        return 0;
    }

    lua_pop( L,1 );
    return 1;
}

#endif /* __LTOOLS_H__ */
