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
    typedef int32 object_id_t;
    typedef int64 raw_factor_t;
    typedef raw_factor_t factor_t[MAX_RANK_FACTOR];
    class object_t
    {
    public:
        object_t(){};
        ~object_t(){};

        inline static int32 compare_factor(
            factor_t src,factor_t dest,int32 max_idx = MAX_RANK_FACTOR)
        {
            for (int32 idx = 0;idx < max_idx;idx ++)
            {
                if (src[idx] == dest[idx]) continue;

                return src[idx] < dest[idx] ? -1 : 1;
            }

            return 0;
        }
    public:
        int32 _index; // 排名
        object_id_t _id;
        factor_t _factor;
    };
public:
    base_rank();
    virtual ~base_rank();

    // 删除一个对象
    virtual int32 remove(object_id_t id) = 0;
    // 插入一个对象
    virtual int32 insert(object_id_t id,factor_t factor,int32 max_idx = 1) = 0;
    // 更新一个对象排序因子
    virtual int32 update(object_id_t id,int64 factor,int32 factor_idx = 0) = 0;

    virtual int32 get_rank_by_id(object_id_t id) const = 0;
    virtual const raw_factor_t *get_factor(object_id_t id) const = 0;
    virtual object_id_t get_id_by_rank(object_id_t rank) const = 0;

    // 清除排行的对象，但不清除内存
    virtual void clear() { _count = 0;_max_factor = 0; }

    /* 设置排行榜上限，超过此上限将不会在排行榜中存数据
     * 排序因子会减小的排行榜谨慎设置此值，因为假如某个对象因为上限不在排行榜，而其他对象排名
     * 下降将会导致该对象不会上榜
     */
    void set_max_count(int32 max) { _max_count = max; };

    // 获取当前排行榜用到的排序因子数量
    int32 get_max_factor() const { return _max_factor; };
    // 获取当前排行榜中对象数量
    int32 get_count() const { return _count; };
protected:
    int32 _count; // 当前排行榜中数量
    int32 _max_count; // 排行榜最大数量
    uint8 _max_factor; // 当前排行榜使用到的最大排序因子数量(从1开始)
};

/* 插入法排序,适用于伤害排行
 * 伤害更新很频繁，但是其实伤害排行榜排名变化不大，移动数组的情况较少
 */
class insertion_rank : public base_rank
{
public:
    insertion_rank();
    ~insertion_rank();

    void clear();

    int32 remove(object_id_t id);
    int32 insert(object_id_t id,factor_t factor,int32 max_idx = 1);
    int32 update(object_id_t id,raw_factor_t factor,int32 factor_idx = 0);

    int32 get_rank_by_id(object_id_t id) const;
    const raw_factor_t *get_factor(object_id_t id) const;
    object_id_t get_id_by_rank(object_id_t rank) const;
private:
    void shift_up(object_t *object);
    void shift_down(object_t *object);
    void raw_remove(object_t *object);
private:
    int32 _max_list;
    object_t **_object_list;
    map_t< object_id_t,object_t* > _object_set;
};

/* 桶排序，只是桶分得比较细，演变成了有序hash map
 * 原本想用std::multimap来实现，但是在C++11之前的版本是无法保证同一个桶中的对象顺序的
 * http://www.cplusplus.com/reference/map/multimap/insert/
 * There are no guarantees on the relative order of equivalent elements
 * 适用场景：一次性大量数据排序，不删除对象，不更新对象，比如全服玩家定时排序
 * 缺点：
 * 1. 占用内存稍大
 * 2. 根据id取排名，根据排名取id都需要重新做一次记录(速度慢)
 */

class bucket_rank : public base_rank
{
public:
    typedef std::list<> bucket_t;
private:
    std::map<> _bucket_set;
};

#endif /* __RANK_H__ */
