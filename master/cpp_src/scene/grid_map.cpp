#include "grid_map.h"
#include "../system/static_global.h"

GridMap::GridMap()
{
    _id = 0;
    _width = 0;
    _height = 0;
    _grid_set = NULL;

    C_OBJECT_ADD("grid_map");
}

GridMap::~GridMap()
{
    delete []_grid_set;

    C_OBJECT_DEC("grid_map");
}

// 加载单个地图文件
bool GridMap::load_file(const char *path)
{
    return true;
}

// 设置地图信息
bool GridMap::set( int32_t id,uint16_t width,uint16_t height )
{
    ASSERT(NULL == _grid_set, "grid map already have data");

    if ( MAX_MAP_GRID < width || MAX_MAP_GRID < height ) return false;

    _id = id;
    _width = width;
    _height = height;
    _grid_set = new int8_t[_width*_height];

    // 全部初始化为不可行走
    memset(_grid_set,-1,sizeof(int8_t)*_width*height);

    return true;
}

// 填充地图信息
bool GridMap::fill( uint16_t x,uint16_t y,int8_t cost )
{
    if ( x >= _width || y >= _height ) return false;

    _grid_set[x*_height + y] = cost;

    return true;
}

// 获取经过这个格子的消耗, < 0 表示不可行
int8_t GridMap::get_pass_cost(int32_t x,int32_t y) const
{
    if ( EXPECT_FALSE(x < 0 || x >= _width) ) return -1;
    if ( EXPECT_FALSE(y < 0 || y >= _height) ) return -1;

    return _grid_set[x*_height + y];
}
