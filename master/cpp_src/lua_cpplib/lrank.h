#pragma once

#include <lua.hpp>

#include "ltools.h"
#include "../util/rank.h"

class linsertion_rank : public insertion_rank
{
public:
    ~linsertion_rank() {};
    explicit linsertion_rank(lua_State *L) {};

    int32 clear(lua_State *L) { insertion_rank::clear();return 0; };

    int32 remove(lua_State *L);
    int32 insert(lua_State *L);
    int32 update(lua_State *L);

    int32 get_count(lua_State *L)
    {
        lua_pushinteger(L,insertion_rank::get_count());
        return 1;
    }

    int32 set_max_count(lua_State *L)
    {
        int32 max_count = luaL_checkinteger(L,1);
        if (max_count <= 0 || max_count > 1024000)
        {
            return luaL_error(L,"max count illegal:%d",max_count);
        }
        insertion_rank::set_max_count(max_count);
        return 0;
    }

    int32 get_max_factor(lua_State *L)
    {
        lua_pushinteger(L,insertion_rank::get_max_factor());
        return 1;
    }

    int32 get_factor(lua_State *L);
    int32 get_rank_by_id(lua_State *L);
    int32 get_id_by_rank(lua_State *L);
};

class lbucket_rank : public bucket_rank
{
public:
    ~lbucket_rank() {};
    explicit lbucket_rank(lua_State *L) {};

    int32 insert(lua_State *L);
    int32 get_top_n(lua_State *L);

    int32 clear(lua_State *L) { bucket_rank::clear();return 0; }
    int32 get_count(lua_State *L)
    {
        lua_pushinteger(L,bucket_rank::get_count());return 1;
    }
};
