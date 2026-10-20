#pragma once

#include "lua.hpp"
#include "global/global.hpp"

namespace net
{

/*
 * 从lua栈读一段二进制数据，统一 string / lightuserdata 两种入参
 *
 *   string        长度取字符串自身，len_idx 处的参数可以缺省；
 *                 如果给了就必须与实际长度一致（防调用方传错长度）
 *   lightuserdata 必须在 len_idx 处给显式长度（指针本身不带长度）
 *
 * 注意：判类型只能用 lua_type，不能用 luaL_checklstring。
 *       后者会把数字实参就地转成字符串、改掉栈槽内容。
 *       另外 lua_tolstring 对 lightuserdata 返回 nullptr，也不能用来判类型。
 *
 * @return 数据指针，空包（nil 或空串）返回 nullptr 且 len = 0
 */
inline const char *read_buf(lua_State *L, int32_t idx, int32_t len_idx,
                            int32_t &len)
{
    len = 0;
    if (lua_isnoneornil(L, idx)) return nullptr;

    const int32_t t = lua_type(L, idx);

    if (LUA_TSTRING == t)
    {
        size_t n      = 0;
        const char *p = lua_tolstring(L, idx, &n);

        if (!lua_isnoneornil(L, len_idx)) // 长度可省，给了就必须一致
        {
            lua_Integer given = luaL_checkinteger(L, len_idx);
            if (given < 0 || (size_t)given != n)
            {
                luaL_error(L, "buffer length mismatch at arg %d: given %d, "
                              "actual %d",
                           idx, (int32_t)given, (int32_t)n);
                return nullptr;
            }
        }

        if (n > 0x7FFFFFFF) luaL_error(L, "buffer too long at arg %d", idx);

        len = (int32_t)n;
        return p;
    }

    if (LUA_TLIGHTUSERDATA == t)
    {
        const char *p = (const char *)lua_touserdata(L, idx);
        if (!p) return nullptr; // 空包也允许用 nullptr + 0 表示

        if (lua_isnoneornil(L, len_idx))
        {
            luaL_error(L, "lightuserdata at arg %d needs explicit length at "
                          "arg %d",
                       idx, len_idx);
            return nullptr;
        }

        lua_Integer n = luaL_checkinteger(L, len_idx);
        if (n < 0 || n > 0x7FFFFFFF)
        {
            luaL_error(L, "invalid buffer length at arg %d", len_idx);
            return nullptr;
        }

        len = (int32_t)n;
        return p;
    }

    luaL_error(L, "expect string or lightuserdata at arg %d", idx);
    return nullptr;
}

} // namespace net
