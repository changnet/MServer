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
    typedef std::vector< entity_id_t > entity_vector_t; // 实体列表

    struct entity_ctx
    {
        uint8 _type; // 记录实体类型
        uint8 _pos_x; // 格子坐标，x
        uint8 _pos_y; // 格子坐标，y
        entity_id_t _id;
        // 关注我的实体列表。比如我周围的玩家，需要看到我移动、放技能
        // 都需要频繁广播给他们。如果游戏并不是arpg，可能并不这个列表
        entity_vector_t* _watch_me;
    };
public:
    grid_aoi();
    virtual ~grid_aoi();

    void set_watch_mask(uint32 mask); // 设置需要放入watch_me列表的实体类型
    void set_visual_range(int32 width,int32 height); // 设置视野
    void set_size(int32 width,int32 height,int32 pix); // 设置宽高，格子像素

    int32 get_all_entitys(entity_vector_t *list);
    /* 获取某一范围内实体
     * 底层这里只支持矩形，如果是其他形状的，上层根据实体位置再筛选即可
     */
    int32 get_entitys(entity_vector_t *list,
        int32 srcx,int32 srcy,int32 destx,int32 desty);

    bool exit_entity(entity_id_t id);
    bool enter_entity(entity_id_t id,int32 x,int32 y,uint8 type);
    bool update_entity(entity_id_t id,int32 x,int32 y,uint8 type);
protected:
    void del_entity_vector(); // 需要实现缓存，太大的直接删除不要丢缓存
    entity_vector_t *new_entity_vector();

    void del_entity_ctx();
    struct entity_ctx *new_entity_ctx();
private:
    /* 需要放入watch_me列表的实体类型，按位取值
     * 一般来说只有玩家需要放进去，因为实体移动、攻击等需要广播给这些玩家
     * 一些游戏或者手游全屏广播的场景，不需要开启这个列表
     */
    uint32 _watch_mask;

    uint8 _width; // 场景最大宽度(格子坐标)
    uint8 _height; // 场景最大高度(格子坐标)
    uint32 _grid_pix; // 多少像素转换为一个格子边长
    uint8 _visual_width; // 视野宽度格子数
    uint8 _visual_height; // 视野高度格子数

    /* 记录每个格子中的实体id列表
     * 格子的x、y坐标为uint16，一起拼成一个uint32作为key
     */
    map_t< uint32,entity_vector_t* > _entity_grid;

    /* 记录所有实体的数据 */
    map_t< entity_id_t,struct entity_ctx* > _entity_set;
};

#endif /* __GRID_AOI_H__ */
