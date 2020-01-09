#pragma once

#include <lua.hpp>

#include "ltools.h"
#include "../util/rank.h"

/**
 * 插入法排序
 */
class LInsertionRank : public insertion_rank
{
public:
    ~LInsertionRank() {}
    explicit LInsertionRank(lua_State *L) {}

    /**
     * 重置排行榜，但不释放内存
     */
    int32_t clear(lua_State *L)
    {
        insertion_rank::clear();
        return 0;
    }

    /**
     * 删除某个元素
     * @param id 元素Id
     * @return 是否成功
     */
    int32_t remove(lua_State *L);

    /**
     * 排入一个排序元素，如果失败，则抛出错误
     * @param id 元素id
     * @param factor 排序因子
     * @param ... 其他排序因子
     */
    int32_t insert(lua_State *L);

    /**
     * 更新元素排序因子
     * @param id 元素id
     * @param factor 排序因子
     * @param index 可选参数，排序因子索引从1开始，不传则为第一个
     * @return 是否成功
     */
    int32_t update(lua_State *L);

    /**
     * 获取排行榜数量
     */
    int32_t get_count(lua_State *L)
    {
        lua_pushinteger(L, insertion_rank::get_count());
        return 1;
    }

    /**
     * 设置排行榜最大数量，默认64
     * 排名在最大数量之后的元素将被丢弃并且排行榜数量减少后也不会回到排行榜
     * @param max_count 最大数量
     */
    int32_t set_max_count(lua_State *L)
    {
        int32_t max_count = luaL_checkinteger(L, 1);
        if (max_count <= 0 || max_count > 1024000)
        {
            return luaL_error(L, "max count illegal:%d", max_count);
        }
        insertion_rank::set_max_count(max_count);
        return 0;
    }

    /**
     * 获取当前排行榜用到的排序因子最大索引，从1开始
     */
    int32_t get_max_factor(lua_State *L)
    {
        lua_pushinteger(L, insertion_rank::get_max_factor());
        return 1;
    }

    /**
     * 获取某个元素的排序因子
     * @param id 元素id
     * @return 排序因子，排序因子1，...
     */
    int32_t get_factor(lua_State *L);

    /**
     * 根据元素id获取排名
     * @param id 元素id
     * @return 返回排名(从1开始)，没有排名则返回-1
     */
    int32_t get_rank_by_id(lua_State *L);

    /**
     * 根据排名获取元素id
     * @param rank 排名，从1开始
     * @return 元素id，无则返回-1
     */
    int32_t get_id_by_rank(lua_State *L);
};

/**
 * 桶排序
 */
class lBucketRank : public bucket_rank
{
public:
    ~lBucketRank() {}
    explicit lBucketRank(lua_State *L) {}

    /**
     * 更新元素排序因子
     * @param id 元素id
     * @param factor 排序因子
     * @param index 可选参数，排序因子索引从1开始，不传则为第一个
     * @return 是否成功
     */
    int32_t insert(lua_State *L);

    /**
     * 获取排行榜前N个元素id
     * @param n 前N个元素
     * @param tbl 获取的元素id存放在该表中，数量在n字段
     */
    int32_t get_top_n(lua_State *L);

    /**
     * 重置排行榜，但不释放内存
     */
    int32_t clear(lua_State *L)
    {
        bucket_rank::clear();
        return 0;
    }

    /**
     * 获取排行榜数量
     */
    int32_t get_count(lua_State *L)
    {
        lua_pushinteger(L, bucket_rank::get_count());
        return 1;
    }
};
