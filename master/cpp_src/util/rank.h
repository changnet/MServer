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
    void clear() { _rank_count = 0; }

    /* 设置排行榜上限，超过此上限将不会在排行榜中存数据
     * 排序因子会减小的排行榜谨慎设置此值，因为假如某个对象因为上限不在排行榜，而其他对象排名
     * 下降将会导致该对象不会上榜
     */
    void set_max_count(int32 max) { _max_count = max; };

    // 获取当前排行榜用到的排序因子数量
    int32 get_max_factor() const { return _max_factor; };
    // 获取当前排行榜中对象数量
    int32 get_rank_count() const { return _rank_count; };
protected:
    int32 _max_count; // 排行榜最大数量
    int32 _rank_count; // 当前排行榜中数量
    uint8 _max_factor; // 当前排行榜使用到的最大排序因子数量
};

// 插入法排序
class insertion_rank : public base_rank
{
public:
    insertion_rank();
    ~insertion_rank();

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
    object_t **_object_list;
    map_t< object_id_t,object_t* > _object_set;
};

#endif /* __RANK_H__ */
