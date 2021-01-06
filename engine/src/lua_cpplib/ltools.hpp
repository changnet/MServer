#pragma once

#include "../global/global.hpp"
#include <cmath>
#include <lua.hpp>

/**
 * lua_pushcfunction(L, traceback);也可以，但lua_pushcfunction其实是创建一个
 * 闭包，会占用一部分内存，然后再gc掉。因此在lua定义一个global函数可能更好
 */
#define LUA_PUSHTRACEBACK(L) lua_getglobal(L, "__G_C_TRACKBACK")

#define LUA_REF(x)       \
    if (x) LUA_UNREF(x); \
    x = luaL_ref(L, LUA_REGISTRYINDEX)

#define LUA_UNREF(x)                             \
    do                                           \
    {                                            \
        if (x > 0)                               \
        {                                        \
            luaL_unref(L, LUA_REGISTRYINDEX, x); \
            x = 0;                               \
        }                                        \
    } while (0)

#define lUAL_CHECKTABLE(L, idx)                                   \
    do                                                            \
    {                                                             \
        if (!lua_istable(L, idx))                                 \
        {                                                         \
            return luaL_error(L, "expect table,got %s",           \
                              lua_typename(L, lua_type(L, idx))); \
        }                                                         \
    } while (0)

/* lua 5.3支持int64，在转换为bson时需要区分int32、int64 */
inline int32_t lua_isbit32(int64_t v)
{
    return v >= INT_MIN && v <= INT_MAX;
}

/* 在lua目前的版本中,number >= integer，如果连number都无法支持64,将被强制截断 */
inline void lua_pushint64(lua_State *L, int64_t v)
{
    if (v <= LUA_MAXINTEGER && v >= LUA_MININTEGER)
    {
        lua_pushinteger(L, static_cast<LUA_INTEGER>(v));
    }
    else
    {
        lua_pushnumber(L, static_cast<LUA_NUMBER>(v));
    }
}

/* 判断一个table是否为数组
 * 1).key全为int并且小于等于INT_MAX则为数组
 * 2).在元表指定__array为true则为数组
 */
inline int32_t lua_isarray(lua_State *L, int32_t index, int32_t *array,
                                  int32_t *max_index)
{
    double key = 0;
    ASSERT(array && max_index, "lua_isarray empty input argument");

    /* set default value */
    *array     = 0;
    *max_index = -1;

    if (luaL_getmetafield(L, index, "__array"))
    {
        if (LUA_TNIL != lua_type(L, -1))
        {
            if (lua_toboolean(L, -1))
            {
                *array = 1;
            }
            else
            {
                lua_pop(L, 1);
                return 0; /* it's object,use default value */
            }
        }

        lua_pop(L, 1); /* pop metafield value */
    }

    /* get max array index */
    lua_pushnil(L);
    while (lua_next(L, index) != 0)
    {
        /* stack status:table, key, value */
        /* array index must be interger and >= 1 */
        if (lua_type(L, -2) != LUA_TNUMBER)
        {
            *max_index = -1;
            lua_pop(L, 2); /* pop both key and value */
            return 0;
        }

        key = lua_tonumber(L, -2);
        if (floor(key) != key || key < 1)
        {
            *max_index = -1;
            lua_pop(L, 2);
            return 0;
        }

        if (key > INT_MAX) /* array index over INT_MAX,must be object */
        {
            *array     = 0;
            *max_index = -1;
            lua_pop(L, 2);

            return 0;
        }

        if (key > *max_index) *max_index = (int)key;

        lua_pop(L, 1);
    }

    if (*max_index > 0) *array = 1;

    return 0;
}

/* 调试函数，打印当前lua 虚拟机栈 */
inline void stack_dump(lua_State *L)
{
    int i;
    int top = lua_gettop(L);
    for (i = 1; i <= top; i++)
    { /* repeat for each level */
        int t = lua_type(L, i);
        switch (t)
        {

        case LUA_TSTRING: /* strings */
            printf("`%s'", lua_tostring(L, i));
            break;

        case LUA_TBOOLEAN: /* booleans */
            printf(lua_toboolean(L, i) ? "true" : "false");
            break;

        case LUA_TNUMBER: /* numbers */ printf("%g", lua_tonumber(L, i)); break;

        default: /* other values */ printf("%s", lua_typename(L, t)); break;
        }
        printf("  "); /* put a separator */
    }
    printf("\n"); /* end the listing */
}

/* debug.traceback in c */
inline int traceback(lua_State *L)
{
    const char *msg = lua_tostring(L, 1);
    if (msg)
    {
        luaL_traceback(L, L, msg, 1);
    }
    else
    {
        if (!lua_isnoneornil(L, 1)) /* is there an error object? */
        {
            if (!luaL_callmeta(
                    L, 1, "__tostring")) /* try its 'tostring' metamethod */
            {
                lua_pushliteral(L, "(no error message)");
            }
        }
    }

    return 1;
}

/**
 * @brief 设置table的n值
 * @param L Lua虚拟机
 * @param index table所在栈索引
 * @param n 数量n
 */
inline void table_pack_size(lua_State *L, int32_t index, int32_t n)
{
    lua_pushstring(L, "n");
    lua_pushinteger(L, n);
    lua_rawset(L, index);
}

/**
 * 类似于table.pack，把一个数组或map打包到table中，n表示数量
 * @param filter 把需要打包的数据放到堆栈并返回true
 */
template <typename Container, typename Filter>
void table_pack(lua_State *L, int32_t index,const Container &container, Filter &&filter)
{
    int32_t n = 0;
    for (auto &iter : container)
    {
        if (filter(iter))
        {
            ++n;
            lua_rawseti(L, index, n);
        }
    }

    table_pack_size(L, index, n);
}
