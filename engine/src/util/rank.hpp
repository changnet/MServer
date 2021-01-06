#pragma once

#include "../global/global.hpp"
#include <list>
#include <map>

/* 排行榜底层数据结构
 * 1. 脚本不太好处理多因子排序和内存的精细管理(如批量移动)，所以放底层实现
 * 2. 只提供排序，不提供数据存储。如果排行榜还有其他数据，在脚本另外处理
 * 3. 提供排行榜接口而不是排序接口
 */

#define MAX_RANK_FACTOR 3 // 最大排序因子数量

class BaseRank
{
public:
    typedef int32_t object_id_t;
    typedef int64_t raw_factor_t;
    typedef raw_factor_t factor_t[MAX_RANK_FACTOR];
    class Object
    {
    public:
        Object() {}
        ~Object() {}

        inline static int32_t compare_factor(const factor_t src,
                                             const factor_t dest,
                                             int32_t max_idx = MAX_RANK_FACTOR)
        {
            for (int32_t idx = 0; idx < max_idx; idx++)
            {
                if (src[idx] == dest[idx]) continue;

                return src[idx] < dest[idx] ? -1 : 1;
            }

            return 0;
        }

    public:
        int32_t _index; // 排名
        object_id_t _id;
        factor_t _factor;
    };

public:
    BaseRank();
    virtual ~BaseRank();

    // 清除排行的对象，但不清除内存
    virtual void clear() { _count = 0; }

    // 获取当前排行榜中对象数量
    int32_t get_count() const { return _count; }

protected:
    int32_t _count; // 当前排行榜中数量
};

/* 插入法排序,适用于伤害排行
 * 伤害更新很频繁，但是其实伤害排行榜排名变化不大，移动数组的情况较少
 * 而且这个排行榜需要发送给前端，因此get_rank_by_id和get_id_by_rank效率都要比较高
 */
class insertion_rank : public BaseRank
{
public:
    insertion_rank();
    ~insertion_rank();

    void clear();

    int32_t remove(object_id_t id);
    int32_t insert(object_id_t id, factor_t factor, int32_t max_idx = 1);
    int32_t update(object_id_t id, raw_factor_t factor, int32_t factor_idx = 0);

    /* 设置排行榜上限，超过此上限将不会在排行榜中存数据
     * 排序因子会减小的排行榜谨慎设置此值，因为假如某个对象因为上限不在排行榜，而其他对象排名
     * 下降将会导致该对象不会上榜
     */
    void set_max_count(int32_t max) { _max_count = max; };

    // 获取当前排行榜用到的排序因子数量
    int32_t get_max_factor() const { return _max_factor; };

    int32_t get_rank_by_id(object_id_t id) const;
    const raw_factor_t *get_factor(object_id_t id) const;
    object_id_t get_id_by_rank(object_id_t rank) const;

private:
    void shift_up(Object *object);
    void shift_down(Object *object);
    void raw_remove(Object *object);

private:
    int32_t _max_count; // 排行榜最大数量
    uint8_t _max_factor; // 当前排行榜使用到的最大排序因子数量(从1开始)

    int32_t _max_list;
    Object **_object_list;
    StdMap<object_id_t, Object *> _object_set;
};

/* 桶排序，只是桶分得比较细，演变成了有序hash map
 * 原本想用std::multimap来实现，但是在C++11之前的版本是无法保证同一个桶中的对象顺序的
 * http://www.cplusplus.com/reference/map/multimap/insert/
 * There are no guarantees on the relative order of equivalent elements
 * 适用场景：一次性大量数据排序，不删除对象，不更新对象，比如全服玩家定时排序
 * 缺点：
 * 1. 占用内存稍大
 * 2. 根据id取排名，根据排名取id都慢。排完序可考虑做缓存
 */

class bucket_rank : public BaseRank
{
public:
    typedef std::list<object_id_t> bucket_t;
    typedef bool (*key_comp_t)(const factor_t src, const factor_t dest);
    typedef std::map<raw_factor_t *, bucket_t, key_comp_t> bucket_list_t;

public:
    bucket_rank();
    ~bucket_rank();

    void clear();
    int32_t insert(object_id_t id, factor_t factor);

    // strict weak order,operator <
    static bool key_comp(const factor_t src, const factor_t dest)
    {
        // TODO:这里暂时没法按_max_factor来对比排序因子，只能全部都对比
        return Object::compare_factor(src, dest) < 0;
    }

protected:
    bucket_list_t _bucket_list;
};
