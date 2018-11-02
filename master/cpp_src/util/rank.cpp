#include "rank.h"

base_rank::base_rank()
{
    _max_count = 64; // 排行榜最大数量(默认64)
    _rank_count = 0; // 当前排行榜中数量
    _max_factor = 0; // 当前排行榜使用到的最大排序因子数量
}

// =========================== insertion rank ================================
insertion_rank::insertion_rank()
{
    _objects = NULL;
}

insertion_rank::~insertion_rank()
{
    delete []_objects;
    _objects = NULL;
}

int32 insertion_rank::remove(int32 id);
int32 insertion_rank::insert(int32 id,factor_t factor);
int32 insertion_rank::update(int32 id,int64 factor,int32 factor_idx);

int32 insertion_rank::get_rank_by_id(int32 id);
factor_t &insertion_rank::get_factor(int32 id);
int32 insertion_rank::get_id_by_rank(int32 rank);
int32 insertion_rank::get_top(int64 *list,int32 n);