#pragma once

#include <lua.hpp>
#include "../scene/grid_map.h"

class lmap : public grid_map
{
public:
    ~lmap();
    explicit lmap( lua_State *L );

    int32_t load( lua_State *L ); // 加载地图数据
    int32_t set( lua_State *L ); // 设置地图信息(用于动态创建地图)
    int32_t fill( lua_State *L ); // (用于动态创建地图)
    int32_t fork( lua_State *L ); // 复制一份地图(用于动态修改地图数据)
    int32_t get_size( lua_State *L ); // 获取地图宽高
    int32_t get_pass_cost( lua_State *L ); // 获取通过某个格子的消耗
};
