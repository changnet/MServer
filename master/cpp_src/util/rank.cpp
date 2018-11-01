#include "rank.h"

base_rank::base_rank()
{
    _max_count = 0; // 排行榜最大数量
    _rank_count = 0; // 当前排行榜中数量
    _max_factor = 0; // 当前排行榜使用到的最大排序因子数量
}