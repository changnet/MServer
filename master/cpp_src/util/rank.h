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
    typedef int64 factor_t[MAX_RANK_FACTOR];
    class object_t
    {
    public:
        object_t(){};
        ~object_t(){};
        object_t(const object_t &obj)
        {
            _id = obj._id;
            for (int32 idx = 0;idx < MAX_RANK_FACTOR;idx ++)
            {
                _factor[idx] = obj._factor[idx];
            }
        }

        // 多因子对比
        inline int32 compre(
            const object_t *src,const object_t *dest,int32 max_idx)
        {
            for (int32 idx = 0;idx < max_idx;idx ++)
            {
                if (src->_factor[idx] == dest->_factor[idx]) continue;

                return src->_factor[idx] < dest->_factor[idx] ? -1 : 1;
            }

            return 0;
        }
    public:
        int32 _id;
        factor_t _factor;
    };
public:
    base_rank();
    virtual ~base_rank();

    // 删除一个对象
    virtual int32 remove(int32 id) = 0;
    // 插入一个对象
    virtual int32 insert(int32 id,factor_t factor) = 0;
    // 更新一个对象排序因子
    virtual int32 update(int32 id,int64 factor,int32 factor_idx = 0) = 0;

    virtual int32 get_rank_by_id(int32 id) = 0;
    virtual factor_t &get_factor(int32 id) = 0;
    virtual int32 get_id_by_rank(int32 rank) = 0;
    virtual int32 get_top(int64 *list,int32 n) = 0;

    // 清除排行的对象，但不清除内存
    void clear() { _rank_count = 0; }
    // 设置排行榜上限，超过此上将不会进入排行榜
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

    int32 remove(int32 id);
    int32 insert(int32 id,factor_t factor);
    int32 update(int32 id,int64 factor,int32 factor_idx = 0);

    int32 get_rank_by_id(int32 id);
    factor_t &get_factor(int32 id);
    int32 get_id_by_rank(int32 rank);
    int32 get_top(int64 *list,int32 n);
private:
    object_t **_object_list;
    map_t< int32,object_t* > _object_set;
};

#endif /* __RANK_H__ */
