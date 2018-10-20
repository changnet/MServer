#include "lgrid_aoi.h"

lgrid_aoi::~lgrid_aoi()
{
}

lgrid_aoi::lgrid_aoi( lua_State *L )
{
}

int32 lgrid_aoi::set_watch_mask( lua_State *L ) // 设置需要放入watch_me列表的实体类型
{
    int32 mask = luaL_checkinteger(L,1);
    grid_aoi::set_watch_mask(mask);

    return 0;
}

int32 lgrid_aoi::set_visual_range( lua_State *L ) // 设置视野
{
    // 这里的宽高都是指格子数
    int32 width = luaL_checkinteger(L,1);
    int32 height = luaL_checkinteger(L,2);
    grid_aoi::set_visual_range(width,height);

    return 0;
}

int32 lgrid_aoi::set_size( lua_State *L ) // 设置宽高，格子像素
{
    // 这里的宽高都是指像素，因为地图的大小可能并不刚好符合格子数，后面再做转换
    int32 width = luaL_checkinteger(L,1);
    int32 height = luaL_checkinteger(L,2);
    int32 pix = luaL_checkinteger(L,3);
    grid_aoi::set_size(width,height,pix);

    return 0;
}

int32 lgrid_aoi::get_all_entitys(lua_State *L)
{
    return 0;
}

/* 获取某一范围内实体
 * 底层这里只支持矩形，如果是其他形状的，上层根据实体位置再筛选即可
 */
int32 lgrid_aoi::get_entitys( lua_State *L )
{
    return 0;
}

int32 lgrid_aoi::exit_entity( lua_State *L )
{
    return 0;
}

int32 lgrid_aoi::enter_entity( lua_State *L )
{
    return 0;
}

int32 lgrid_aoi::update_entity( lua_State *L )
{
    return 0;
}
