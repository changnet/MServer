#pragma once

#include "../pool/object_pool.h"
#include <vector>

/**
 * 格子AOI(Area of Interest)算法
 * 2018-07-18 by xzc
 *
 * 1. 暂定所有实体视野一样
 * 2. 按mmoarpg设计，需要频繁广播技能、扣血等，因此需要watch_me列表
 * 3. 只提供获取矩形范围内实体。技能寻敌时如果不是矩形，上层再从这些实体筛选
 * 4. 通过event来控制实体关注的事件。npc、怪物通常不关注任何事件，这样可以大幅提升
 *    效率，战斗ai另外做即可(攻击玩家在ai定时器定时取watch_me列表即可)。怪物攻击怪物或
 *    npc在玩家靠近时对话可以给这些实体加上事件，这样的实体不会太多
 * 5. 假设1m为一个格子，则1平方千米的地图存实体指针的内存为 1024 * 1024 * 8 = 8k
 */
class GridAOI
{
public:
    struct entity_ctx;
    using entity_id_t     = int64_t; // 用来标识实体的唯一id
    using entity_vector_t = std::vector<struct entity_ctx *>; // 实体列表
    using entity_set_t = std::unordered_map<entity_id_t, struct entity_ctx *>;

    /**
     * 场景中单个实体的类型、坐标等数据
     */
    struct entity_ctx
    {
        uint8_t _type;  // 记录实体类型
        uint8_t _event; // 关注的事件
        uint16_t _pos_x; // 格子坐标，x
        uint16_t _pos_y; // 格子坐标，y
        entity_id_t _id;
        // 关注我的实体列表。比如我周围的玩家，需要看到我移动、放技能
        // 都需要频繁广播给他们。如果游戏并不是arpg，可能并不需要这个列表
        // 一般怪物、npc不要加入这个列表，如果有少部分npc需要aoi事件，另外定一个类型
        entity_vector_t *_watch_me;
    };

public:
    GridAOI();
    virtual ~GridAOI();

    bool set_visual_range(int32_t width, int32_t height);
    void set_size(int32_t width, int32_t height, int32_t pix_grid);

    struct entity_ctx *get_entity_ctx(entity_id_t id);
    /**
     * 获取某一范围内实体。底层这里只支持矩形，如果是其他形状的，上层根据实体位置再筛选即可
     */
    int32_t get_entitys(entity_vector_t *list, int32_t srcx, int32_t srcy,
                        int32_t destx, int32_t desty);

    int32_t exit_entity(entity_id_t id, entity_vector_t *list = NULL);
    int32_t enter_entity(entity_id_t id, int32_t x, int32_t y, uint8_t type,
                         uint8_t event, entity_vector_t *list = NULL);
    int32_t update_entity(entity_id_t id, int32_t x, int32_t y,
                          entity_vector_t *list     = NULL,
                          entity_vector_t *list_in  = NULL,
                          entity_vector_t *list_out = NULL);

protected:
    entity_vector_t *new_entity_vector();
    void del_entity_vector(entity_vector_t *list);

    struct entity_ctx *new_entity_ctx();
    void del_entity_ctx(struct entity_ctx *ctx);

private:
    bool remove_entity_from_vector(entity_vector_t *list,
                                   const struct entity_ctx *ctx);
    /** 插入实体到格子内 */
    void insert_grid_entity(int32_t x, int32_t y, struct entity_ctx *ctx);
    /** 从格子列表内删除实体 */
    bool remove_grid_entity(int32_t x, int32_t y, const struct entity_ctx *ctx);
    // 获取矩形内的实体
    int32_t raw_get_entitys(entity_vector_t *list, int32_t x, int32_t y,
                            int32_t dx, int32_t dy);
    // 判断视野范围
    void get_visual_range(int32_t &x, int32_t &y, int32_t &dx, int32_t &dy,
                          int32_t pos_x, int32_t pos_y);
    // 处理实体进入某个范围
    void entity_enter_range(struct entity_ctx *ctx, int32_t x, int32_t y,
                            int32_t dx, int32_t dy, entity_vector_t *list = NULL);
    // 处理实体退出某个范围
    void entity_exit_range(struct entity_ctx *ctx, int32_t x, int32_t y,
                           int32_t dx, int32_t dy, entity_vector_t *list = NULL);

private:
    typedef ObjectPool<GridAOI::entity_ctx, 10240, 1024> ctx_pool_t;
    typedef ObjectPool<GridAOI::entity_vector_t, 10240, 1024> vector_pool_t;

    ctx_pool_t *get_ctx_pool()
    {
        static thread_local ctx_pool_t ctx_pool("grid_aoi_ctx");

        return &ctx_pool;
    }
    vector_pool_t *get_vector_pool()
    {
        static thread_local vector_pool_t ctx_pool("grid_aoi_vector");

        return &ctx_pool;
    }

protected:
    int32_t _width;  // 场景最大宽度(格子坐标)
    int32_t _height; // 场景最大高度(格子坐标)
    int32_t _pix_grid; // 每个格子表示的像素大小

    // 格子数指以实体为中心，不包含当前格子，上下或者左右的格子数
    int32_t _visual_width;  // 视野宽度格子数
    int32_t _visual_height; // 视野高度格子数

    /**
     * 记录每个格子中的实体id列表
     * 有X[m][n]、X[_width * m + n]这两种存储方式，测试后发现第二种效率更高
     * https://stackoverflow.com/questions/936687/how-do-i-declare-a-2d-array-in-c-using-new
     */
    entity_vector_t *_entity_grid;

    /* 记录所有实体的数据 */
    entity_set_t _entity_set;
};
