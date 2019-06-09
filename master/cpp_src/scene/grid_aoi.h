/* 九宫格AOI(Area of Interest)算法
 * 2018-07-18 by xzc
 *
 * 1. 暂定所有实体视野一样
 * 2. 按mmoarpg设计，需要频繁广播技能、扣血等，因此需要watch_me列表
 * 3. 只提供获取矩形范围内实体。技能寻敌时如果不是矩形，上层再从这些实体筛选
 * 4. 通过event来控制实体关注的事件。npc、怪物通常不关注任何事件，这样可以大幅提升
 *    效率，战斗ai另外做即可(攻击玩家在ai定时器定时取watch_me列表即可)。怪物攻击怪物或
 *    npc在玩家靠近时对话可以给这些实体加上事件，这样的实体不会太多
 */

#ifndef __GRID_AOI_H__
#define __GRID_AOI_H__

#include <vector>
#include "../pool/object_pool.h"

class grid_aoi
{
public:
    struct entity_ctx;

    typedef int64 entity_id_t; // 用来标识实体的唯一id
    typedef std::vector< struct entity_ctx* > entity_vector_t; // 实体列表

    struct entity_ctx
    {
        uint8 _type; // 记录实体类型
        uint8 _event; // 关注的事件
        uint8 _pos_x; // 格子坐标，x
        uint8 _pos_y; // 格子坐标，y
        entity_id_t _id;
        // 关注我的实体列表。比如我周围的玩家，需要看到我移动、放技能
        // 都需要频繁广播给他们。如果游戏并不是arpg，可能并不需要这个列表
        // 一般怪物、npc不要加入这个列表，如果有少部分npc需要aoi事件，另外定一个类型
        entity_vector_t* _watch_me;
    };

    typedef map_t< entity_id_t,struct entity_ctx* > entity_set_t;
public:
    grid_aoi();
    virtual ~grid_aoi();

    void set_visual_range(int32 width,int32 height); // 设置视野
    int32 set_size(int32 width,int32 height); // 设置宽高

    struct entity_ctx *get_entity_ctx(entity_id_t id);
    /* 获取某一范围内实体
     * 底层这里只支持矩形，如果是其他形状的，上层根据实体位置再筛选即可
     */
    int32 get_entitys(entity_vector_t *list,
        int32 srcx,int32 srcy,int32 destx,int32 desty);

    int32 exit_entity(entity_id_t id,entity_vector_t *list = NULL);
    int32 enter_entity(entity_id_t id,
        int32 x,int32 y,uint8 type,uint8 event,entity_vector_t *list = NULL);
    int32 update_entity(entity_id_t id,
        int32 x,int32 y,entity_vector_t *list = NULL,
        entity_vector_t *list_in = NULL,entity_vector_t *list_out = NULL);
protected:
    entity_vector_t *new_entity_vector();
    void del_entity_vector(entity_vector_t *list);

    struct entity_ctx *new_entity_ctx();
    void del_entity_ctx(struct entity_ctx *ctx);
private:
    entity_vector_t *get_grid_entitys(int32 x,int32 y); // 获取格子内的实体列表
    bool remove_entity_from_vector(
        entity_vector_t *list,const struct entity_ctx *ctx);
    // 插入实体到格子内
    void insert_grid_entity(int32 x,int32 y,struct entity_ctx *ctx);
    // 删除格子内实体
    bool remove_grid_entity(int32 x,int32 y,const struct entity_ctx *ctx);
    // 获取矩形内的实体
    int32 raw_get_entitys(
        entity_vector_t *list,int32 x,int32 y,int32 dx,int32 dy);
    // 判断视野范围
    void get_visual_range(
        int32 &x,int32 &y,int32 &dx,int32 &dy,int32 pos_x,int32 pos_y);
    // 处理实体进入某个范围
    void entity_enter_range(struct entity_ctx *ctx,
        int32 x,int32 y,int32 dx,int32 dy,entity_vector_t *list = NULL);
    // 处理实体退出某个范围
    void entity_exit_range(struct entity_ctx *ctx,
        int32 x,int32 y,int32 dx,int32 dy,entity_vector_t *list = NULL);
private:
    typedef object_pool< grid_aoi::entity_ctx,10240,1024 > ctx_pool_t;
    typedef object_pool< grid_aoi::entity_vector_t,10240,1024 > vector_pool_t;

    ctx_pool_t *get_ctx_pool()
    {
        static ctx_pool_t ctx_pool("grid_aoi_ctx");

        return &ctx_pool;
    }
    vector_pool_t *get_vector_pool()
    {
        static vector_pool_t ctx_pool("grid_aoi_vector");

        return &ctx_pool;
    }
protected:
    uint8 _width; // 场景最大宽度(格子坐标)
    uint8 _height; // 场景最大高度(格子坐标)

    // 格子数指以实体为中心，不包含当前格子，上下或者左右的格子数
    uint8 _visual_width; // 视野宽度格子数
    uint8 _visual_height; // 视野高度格子数

    /* 记录每个格子中的实体id列表
     * 格子的x、y坐标为uint16，一起拼成一个uint32作为key
     */
    map_t< uint32,entity_vector_t* > _entity_grid;

    /* 记录所有实体的数据 */
    entity_set_t _entity_set;
};

#endif /* __GRID_AOI_H__ */
