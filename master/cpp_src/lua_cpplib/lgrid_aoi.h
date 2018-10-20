#ifndef __LGRID_AOI_H__
#define __LGRID_AOI_H__

#include <lua.hpp>
#include "../scene/grid_aoi.h"

class lgrid_aoi : public grid_aoi
{
public:
    ~lgrid_aoi();
    explicit lgrid_aoi( lua_State *L );

    int32 set_watch_mask( lua_State *L ); // 设置需要放入watch_me列表的实体类型
    int32 set_visual_range( lua_State *L ); // 设置视野
    int32 set_size( lua_State *L ); // 设置宽高，格子像素

    int32 get_all_entitys(lua_State *L);
    /* 获取某一范围内实体
     * 底层这里只支持矩形，如果是其他形状的，上层根据实体位置再筛选即可
     */
    int32 get_entitys( lua_State *L );

    int32 exit_entity( lua_State *L );
    int32 enter_entity( lua_State *L );
    int32 update_entity( lua_State *L );
};

#endif /* __LGRID_AOI_H__ */
