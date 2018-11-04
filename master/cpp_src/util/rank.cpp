#include "rank.h"

base_rank::base_rank()
{
    _count = 0; // 当前排行榜中数量
    _max_count = 64; // 排行榜最大数量(默认64)
    _max_factor = 0; // 当前排行榜使用到的最大排序因子数量
}

base_rank::~base_rank()
{
}

// =========================== insertion rank ================================
insertion_rank::insertion_rank()
{
    _max_list = 0;
    _object_list = NULL;
}

insertion_rank::~insertion_rank()
{
    clear();

    delete []_object_list;

    _max_list = 0;
    _object_list = NULL;
}

void insertion_rank::clear()
{
    map_t< object_id_t,object_t* >::const_iterator iter = _object_set.begin();
    for (;iter != _object_set.end();iter ++)
    {
        delete iter->second;
    }
    _object_set.clear();

    base_rank::clear();
}

// 把object往数组0位置移动
void insertion_rank::shift_up(object_t *object)
{
    for (int32 idx = object->_index - 1;idx > 0;idx --)
    {
        object_t *next = _object_list[idx];
        if (object_t::compare_factor(
            object->_factor,next->_factor,_max_factor) <= 0)
        {
            return;
        }

        // 交换排名
        next->_index ++;
        object->_index --;

        _object_list[idx] = object;
        _object_list[idx + 1] = next;
    }
}

// 把object往数组尾移动
void insertion_rank::shift_down(object_t *object)
{
    for (int32 idx = object->_index + 1;idx < _count;idx ++)
    {
        object_t *next = _object_list[idx];
        if (object_t::compare_factor(
            object->_factor,next->_factor,_max_factor) >= 0)
        {
            return;
        }

        // 交换排名
        next->_index --;
        object->_index ++;

        _object_list[idx] = object;
        _object_list[idx - 1] = next;
    }
}

// 从排行榜中删除该对象
void insertion_rank::raw_remove(object_t *object)
{
    _count --;
    for (int32 idx = object->_index;idx < _count;idx ++)
    {
        _object_list[idx] = _object_list[idx + 1];
        _object_list[idx]->_index --;
    }

    _object_set.erase(object->_id);
    delete object;
}

int32 insertion_rank::remove(object_id_t id)
{
    map_t< object_id_t,object_t* >::const_iterator iter = _object_set.find(id);
    if (iter == _object_set.end()) return -1;

    raw_remove(iter->second);

    return 0;
}

// 插入一个排序对象，id不可重复
int32 insertion_rank::insert(object_id_t id,factor_t factor,int32 max_idx)
{
    if (expect_false(max_idx <= 0 || max_idx > MAX_RANK_FACTOR)) return 1;

    if (expect_false(_max_count <= 0)) return 2;

    if (expect_false(max_idx > _max_factor)) _max_factor = max_idx;

    // 排行榜已满处理
    if (_count >= _max_count)
    {
        object_t *last = _object_list[_count - 1];

        // 不在排名之内，丢弃
        int32 cmp = object_t::compare_factor(factor,last->_factor,_max_factor);
        if (cmp <= 0) return 0;

        // 删除掉最后一个,腾出一个空位
        raw_remove(last);
    }

    // 防止重复
    std::pair< map_t< object_id_t,object_t* >::iterator,bool > ret;
    ret = _object_set.insert(std::pair<object_id_t,object_t*>(id,NULL));
    if (false == ret.second) return 3;

    object_t *object = new object_t();
    ret.first->second = object;

    object->_id = id;
    for (int32 idx = 0;idx < _max_factor;idx ++)
    {
        object->_factor[idx] = factor[idx];
    }

    array_resize(object_t*,_object_list,_max_list,_max_count,array_zero);

    object->_index = _count;
    _object_list[_count++] = object;

    shift_up(object);

    return 0;
}

// 更新对象排序因子
int32 insertion_rank::update(
    object_id_t id,raw_factor_t factor,int32 factor_idx)
{
    if (expect_false(factor_idx <= 0 || factor_idx > MAX_RANK_FACTOR)) return 1;

    map_t< object_id_t,object_t* >::iterator iter = _object_set.find(id);
    if (iter == _object_set.end()) return 2;

    object_t *object = iter->second;
    int32 raw_idx = factor_idx - 1;
    raw_factor_t old_factor = object->_factor[raw_idx];

    // 在实际应用中，很少会出现同时更新几个排序因子的情况，因此只给单个因子更新接口
    if (old_factor == factor) return 0;
    object->_factor[raw_idx] = factor;
    if (expect_false(factor_idx > _max_factor)) _max_factor = factor_idx;

    old_factor > factor ? shift_down(object) : shift_up(object);
    return 0;
}

// 通过id取排名，返回排名(从1开始),出错返回 -1
int32 insertion_rank::get_rank_by_id(object_id_t id) const
{
    map_t< object_id_t,object_t* >::const_iterator iter = _object_set.find(id);
    if (iter == _object_set.end()) return -1;

    // 返回排名(从1开始)
    return iter->second->_index + 1;
}

// 根据id取排序因子
const base_rank::raw_factor_t *insertion_rank::get_factor(object_id_t id) const
{
    map_t< object_id_t,object_t* >::const_iterator iter = _object_set.find(id);
    if (iter == _object_set.end()) return NULL;

    return iter->second->_factor;
}

//  根据排名获取对象id
base_rank::object_id_t insertion_rank::get_id_by_rank(object_id_t rank) const
{
    if (rank <= 0 || rank > _count) return -1;

    // 排名从1开始
    return _object_list[rank - 1]->_id;
}
