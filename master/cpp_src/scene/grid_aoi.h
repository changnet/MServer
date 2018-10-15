/* 九宫格AOI(Area of Interest)算法
 * 2018-07-18 by xzc
 */

#ifndef __GRID_AOI_H__
#define __GRID_AOI_H__

#include <vector>
#include "../global/global.h"

class grid_aoi
{
public:
    typedef entity_id_t uint32; // 用来标识实体的唯一id
    struct entity_ctx
    {
        uint8 _mask;
        int32 _pos_x;
        int32 _pos_y;
        entity_id_t _id;
        // 关注我的实体列表。比如我周围的玩家，需要看到我移动、放技能
        // 都需要频繁广播给他们。如果游戏并不是arpg，那么可能并不这个列表
        std::vector< entity_id_t > *_watch_me;
    };
public:
    grid_aoi();
    virtual ~grid_aoi();

    bool exit_entity(entity_id_t id);
    bool enter_entity(entity_id_t id,int32 x,int32 y,uint8 mask);
    bool update_entity(entity_id_t id,int32 x,int32 y,uint8 mask);
private:
    map_t< entity_id_t,struct entity_ctx > _entity_set;

};

#endif /* __GRID_AOI_H__ */
