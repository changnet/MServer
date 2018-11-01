#ifndef __RANK_H__
#define __RANK_H__

#include "../global/global.h"

/* 排行榜底层数据结构
 * 1. 脚本不太好处理多因子排序和内存的精细管理(如批量移动)，所以放底层实现
 * 2. 只提供排序，不提供数据存储。如果排行榜还有其他数据，在脚本另外处理
 * 3. 提供排行榜接口而不是排序接口
 */

#define MAX_RANK_FACTOR 3 // 最大排序因子数量

class base_rank
{
public:
    typedef int64[MAX_RANK_FACTOR] factor_t;
public:
    base_rank();
    virtual ~base_rank();

    virtual int32 remove(int32 id) = 0;
    virtual int32 insert(int32 id,factor_t factor) = 0;
    virtual int32 update(int32 id,int64 factor,int32 factor_idx) = 0;

    virtual int32 get_rank_by_id(int32 id) = 0;
    virtual factor_t &get_factor(int32 id) = 0;
    virtual int32 get_id_by_rank(int32 rank) = 0;
    virtual int32 get_top(int64 *list,int32 n) = 0;

    void set_max_count(int32 max) { _max_count = max; };

    int32 get_max_factor() const { return _max_factor; };
    int32 get_rank_count() const { return _rank_count; };
protected:
    int32 _max_count; // 排行榜最大数量
    int32 _rank_count; // 当前排行榜中数量
    uint8 _max_factor; // 当前排行榜使用到的最大排序因子数量
};

class insertion_rank : public base_rank
{
public:
};

#endif /* __RANK_H__ */
