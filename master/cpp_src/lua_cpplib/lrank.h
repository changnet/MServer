#ifndef __LRANK_H__
#define __LRANK_H__

#include <lua.hpp>
#include "../util/rank.h"

template< class T >
class lrank
{
public:
    ~lrank() {};
    explicit lrank(lua_State *L) {};

    int32 clear(lua_State *L) { _rank.clear();return 0; };

    int32 remove(lua_State *L);
    int32 insert(lua_State *L);
    int32 update(lua_State *L);

    int32 get_count(lua_State *L)
    {
        lua_pushinteger(L,_rank.get_count());
        return 1;
    }

    int32 set_max_count(lua_State *L)
    {
        int32 max_count = luaL_checkinteger(L,1);
        if (max_count <= 0 || max_count > 1024000)
        {
            return luaL_error(L,"max count illegal:%d",max_count);
        }
        _rank.set_max_count(max_count);
        return 0;
    }

    int32 get_max_factor(lua_State *L)
    {
        lua_pushinteger(L,_rank.get_max_factor());
        return 1;
    }

    int32 get_factor(lua_State *L);
    int32 get_rank_by_id(lua_State *L);
    int32 get_id_by_rank(lua_State *L);
private:
    T _rank;
};

template< class T > int32 lrank<T>::remove(lua_State *L)
{
    base_rank::object_id_t id = luaL_checkinteger(L,1);

    int32 ret = _rank.remove(id);

    lua_pushboolean(L,ret == 0);
    return 1;
}

template< class T > int32 lrank<T>::insert(lua_State *L)
{
    base_rank::object_id_t id = luaL_checkinteger(L,1);

    // 可以传入一个或多个排序因子，至少传入一个
    int32 factor_idx = 1;
    base_rank::factor_t factor;
    factor[0] = luaL_checkinteger(L,2);
    for (int32 idx = 1;idx < MAX_RANK_FACTOR;idx ++)
    {
        if (!lua_isinteger(L,2 + idx)) break;

        factor[factor_idx++] = lua_tointeger(L,2 + idx);
    }

    int32 ret = _rank.insert(id,factor,factor_idx);
    if (0 != ret)
    {
        return luaL_error(L,"rank insert error,code:%d",ret);
    }

    return 0;
}

template< class T > int32 lrank<T>::update(lua_State *L)
{
    base_rank::object_id_t id = luaL_checkinteger(L,1);
    base_rank::raw_factor_t factor = luaL_checkinteger(L,2);

    int32 factor_idx = luaL_optinteger(L,3,1);

    int32 ret = _rank.update(id,factor,factor_idx);

    // 有可能未能进入排行榜，更新失败
    lua_pushboolean(L,ret == 0);
    return 1;
}

template< class T > int32 lrank<T>::get_factor(lua_State *L)
{
    base_rank::object_id_t id = luaL_checkinteger(L,1);
    const base_rank::raw_factor_t *factor = _rank.get_factor(id);
    if (!factor) return 0;

    int32 max_factor = _rank.get_max_factor();

    lua_checkstack( L,max_factor );
    for (int32 idx = 0;idx < max_factor;idx ++)
    {
        lua_pushinteger(L,factor[idx]);
    }
    return max_factor;
}

template< class T > int32 lrank<T>::get_rank_by_id(lua_State *L)
{
    base_rank::object_id_t id = luaL_checkinteger(L,1);

    lua_pushinteger(L,_rank.get_rank_by_id(id));

    return 1;
}

template< class T > int32 lrank<T>::get_id_by_rank(lua_State *L)
{
    int32 rank = luaL_checkinteger(L,1);

    lua_pushinteger(L,_rank.get_id_by_rank(rank));

    return 1;
}

#endif /* __LRANK_H__ */
