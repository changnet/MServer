#pragma once

#include <lua.hpp>

#include "ltools.h"
#include "../util/rank.h"

class LInsertionRank : public insertion_rank
{
public:
    ~LInsertionRank() {}
    explicit LInsertionRank(lua_State *L) {}

    int32_t clear(lua_State *L) { insertion_rank::clear();return 0; }

    int32_t remove(lua_State *L);
    int32_t insert(lua_State *L);
    int32_t update(lua_State *L);

    int32_t get_count(lua_State *L)
    {
        lua_pushinteger(L,insertion_rank::get_count());
        return 1;
    }

    int32_t set_max_count(lua_State *L)
    {
        int32_t max_count = luaL_checkinteger(L,1);
        if (max_count <= 0 || max_count > 1024000)
        {
            return luaL_error(L,"max count illegal:%d",max_count);
        }
        insertion_rank::set_max_count(max_count);
        return 0;
    }

    int32_t get_max_factor(lua_State *L)
    {
        lua_pushinteger(L,insertion_rank::get_max_factor());
        return 1;
    }

    int32_t get_factor(lua_State *L);
    int32_t get_rank_by_id(lua_State *L);
    int32_t get_id_by_rank(lua_State *L);
};

class lBucketRank : public bucket_rank
{
public:
    ~lBucketRank() {}
    explicit lBucketRank(lua_State *L) {}

    int32_t insert(lua_State *L);
    int32_t get_top_n(lua_State *L);

    int32_t clear(lua_State *L) { bucket_rank::clear();return 0; }
    int32_t get_count(lua_State *L)
    {
        lua_pushinteger(L,bucket_rank::get_count());return 1;
    }
};
