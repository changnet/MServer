#include "grid_map.h"

grid_map::grid_map()
{
    _width = 0;
    _height = 0;
    _grid_set = NULL;
}

grid_map::~grid_map()
{
    delete []_grid_set;
}

// 加载单个地图文件
bool grid_map::load_file(const char *path)
{
    return true;
}

// 获取经过这个格子的消耗, < 0 表示不可行
int8_t grid_map::get_pass_cost(int32_t x,int32_t y) const
{
    if ( expect_false(x < 0 || x >= _width) ) return -1;
    if ( expect_false(y < 0 || y >= _height) ) return -1;

    return _grid_set[x*_height + y];
}
