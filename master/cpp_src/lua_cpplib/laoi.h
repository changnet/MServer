#pragma once

#include <lua.hpp>
#include "../scene/grid_aoi.h"

class laoi : public grid_aoi
{
public:
    ~laoi();
    explicit laoi( lua_State *L );

    int32_t set_size( lua_State *L ); // 设置宽高，格子像素
    int32_t set_visual_range( lua_State *L ); // 设置视野

    int32_t get_all_entitys(lua_State *L);
    int32_t get_watch_me_entitys(lua_State *L);
    /* 获取某一范围内实体
     * 底层这里只支持矩形，如果是其他形状的，上层根据实体位置再筛选即可
     */
    int32_t get_entitys( lua_State *L );

    int32_t exit_entity( lua_State *L );
    int32_t enter_entity( lua_State *L );
    int32_t update_entity( lua_State *L );

    // 两个位置在aoi中是否一致
    int32_t is_same_pos( lua_State *L );
};
