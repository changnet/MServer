#include "grid_map.h"
#include "../system/static_global.h"

grid_map::grid_map()
{
    _id = 0;
    _width = 0;
    _height = 0;
    _grid_set = NULL;

    C_OBJECT_ADD("grid_map");
}

grid_map::~grid_map()
{
    delete []_grid_set;

    C_OBJECT_DEC("grid_map");
}

// 加载单个地图文件
bool grid_map::load_file(const char *path)
{
    return true;
}

// 设置地图信息
bool grid_map::set( int32 id,uint16 width,uint16 height )
{
    assert("grid map already have data", NULL == _grid_set);

    if ( MAX_MAP_GRID < width || MAX_MAP_GRID < height ) return false;

    _id = id;
    _width = width;
    _height = height;
    _grid_set = new int8[_width*_height];

    // 全部初始化为不可行走
    memset(_grid_set,-1,sizeof(int8)*_width*height);

    return true;
}

// 填充地图信息
bool grid_map::fill( uint16 x,uint16 y,int8 cost )
{
    if ( x >= _width || y >= _height ) return false;

    _grid_set[x*_height + y] = cost;

    return true;
}

// 获取经过这个格子的消耗, < 0 表示不可行
int8 grid_map::get_pass_cost(int32 x,int32 y) const
{
    if ( expect_false(x < 0 || x >= _width) ) return -1;
    if ( expect_false(y < 0 || y >= _height) ) return -1;

    return _grid_set[x*_height + y];
}
