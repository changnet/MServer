#include "lrank.h"


int32_t LInsertionRank::remove(lua_State *L)
{
    BaseRank::object_id_t id = luaL_checkinteger(L,1);

    int32_t ret = insertion_rank::remove(id);

    lua_pushboolean(L,ret == 0);
    return 1;
}

int32_t LInsertionRank::insert(lua_State *L)
{
    BaseRank::object_id_t id = luaL_checkinteger(L,1);

    // 可以传入一个或多个排序因子，至少传入一个
    int32_t factor_idx = 1;
    BaseRank::factor_t factor = {0}; // 全部初始化为0
    factor[0] = luaL_checkinteger(L,2);
    for (int32_t idx = 1;idx < MAX_RANK_FACTOR;idx ++)
    {
        // 没有默认为0
        if (!lua_isinteger(L,2 + idx)) break;
        factor[factor_idx++] = lua_tointeger(L,2 + idx);
    }

    int32_t ret = insertion_rank::insert(id,factor,factor_idx);
    if (0 != ret)
    {
        return luaL_error(L,"rank insert error,code:%d",ret);
    }

    return 0;
}

int32_t LInsertionRank::update(lua_State *L)
{
    BaseRank::object_id_t id = luaL_checkinteger(L,1);
    BaseRank::raw_factor_t factor = luaL_checkinteger(L,2);

    int32_t factor_idx = luaL_optinteger(L,3,1);

    int32_t ret = insertion_rank::update(id,factor,factor_idx);

    // 有可能未能进入排行榜，更新失败
    lua_pushboolean(L,ret == 0);
    return 1;
}

int32_t LInsertionRank::get_factor(lua_State *L)
{
    BaseRank::object_id_t id = luaL_checkinteger(L,1);
    const BaseRank::raw_factor_t *factor = insertion_rank::get_factor(id);
    if (!factor) return 0;

    int32_t max_factor = insertion_rank::get_max_factor();

    lua_checkstack( L,max_factor );
    for (int32_t idx = 0;idx < max_factor;idx ++)
    {
        lua_pushinteger(L,factor[idx]);
    }
    return max_factor;
}

int32_t LInsertionRank::get_rank_by_id(lua_State *L)
{
    BaseRank::object_id_t id = luaL_checkinteger(L,1);

    lua_pushinteger(L,insertion_rank::get_rank_by_id(id));

    return 1;
}

int32_t LInsertionRank::get_id_by_rank(lua_State *L)
{
    int32_t rank = luaL_checkinteger(L,1);

    lua_pushinteger(L,insertion_rank::get_id_by_rank(rank));

    return 1;
}

// ========================== bucket rank =================================

// 插入一个排序对象
int32_t lBucketRank::insert(lua_State *L)
{
    BaseRank::object_id_t id = luaL_checkinteger(L,1);

    // 可以传入一个或多个排序因子，至少传入一个
    int32_t factor_idx = 1;
    BaseRank::factor_t factor = {0}; // 全部初始化为0
    factor[0] = luaL_checkinteger(L,2);
    for (int32_t idx = 1;idx < MAX_RANK_FACTOR;idx ++)
    {
        // 没有默认为0
        if (!lua_isinteger(L,2 + idx)) break;
        factor[factor_idx++] = lua_tointeger(L,2 + idx);
    }

    int32_t ret = bucket_rank::insert(id,factor);
    if (0 != ret)
    {
        return luaL_error(L,"rank insert error,code:%d",ret);
    }

    return 0;
}

// 获取前N名对象
int32_t lBucketRank::get_top_n(lua_State *L)
{
    int32_t top_n = luaL_checkinteger(L,1);

    int32_t tbl_idx = 2;
    lUAL_CHECKTABLE(L,tbl_idx);

    // 类似table.pack的做法，最后设置一个n表示数量
    int32_t count = 0;
    bucket_list_t::const_iterator iter = _bucket_list.begin();
    for (;iter != _bucket_list.end();iter ++)
    {
        const bucket_t &bucket = iter->second;
        bucket_t::const_iterator biter = bucket.begin();
        for (;biter != bucket.end();biter ++)
        {
            lua_pushinteger(L,*biter);
            lua_rawseti(L,tbl_idx,++count);

            if (count >= top_n) goto DONE;
        }
    }

DONE:
    lua_pushstring( L,"n" ); 
    lua_pushinteger(L,count);
    lua_rawset(L,tbl_idx);

    lua_pushinteger(L,count);
    return 1;
}
