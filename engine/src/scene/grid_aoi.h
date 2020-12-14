#pragma once

#include "../pool/object_pool.h"
#include <functional>

/**
 * 格子AOI(Area of Interest)算法
 * 2018-07-18 by xzc
 *
 * 1. 暂定所有实体视野一样
 * 2. 按mmoarpg设计，需要频繁广播技能、扣血等，因此需要_interest_me列表
 * 3. 只提供获取矩形范围内实体。技能寻敌时如果不是矩形，上层再从这些实体筛选
 * 4. 通过mask来控制实体之间的交互
 * 5. 假设1m为一个格子，则1平方千米的地图存实体指针的内存为 1024 * 1024 * 8 = 8k
 *    (实际是一个vector链表，要大些，但一个服的地图数量有限，应该是可接受的)
 * 6. 所有对外接口均为像素，内部则转换为格子坐标来处理，但不会记录原有的像素坐标
 */
class GridAOI
{
public:
    struct EntityCtx;
    using EntityId     = int64_t; // 用来标识实体的唯一id
    using EntityVector = std::vector<struct EntityCtx *>; // 实体列表
    using EntitySet    = std::unordered_map<EntityId, struct EntityCtx *>;

    /**
     * 场景中单个实体的类型、坐标等数据
     */
    struct EntityCtx
    {
        /**
         * @brief 掩码，按位表示，由上层定义
         *
         */
        uint8_t _mask;

        uint16_t _pos_x; // 格子坐标，x
        uint16_t _pos_y; // 格子坐标，y
        EntityId _id;

        /**
         * 对我感兴趣的实体列表。比如我周围的玩家，需要看到我移动、放技能，都需要频繁广播给他们，
         * 可以直接从这个列表取。如果游戏并不是arpg，可能并不需要这个列表。
         * 当两个实体的mask存在交集时，将同时放入双方的_interest_me，但这只是做初步的筛选，例
         * 如不要把怪物放到玩家的_interest_me，但并不能处理玩家隐身等各种复杂的逻辑，还需要上层
         * 根据业务处理
         */
        EntityVector *_interest_me;
    };

public:
    GridAOI();
    virtual ~GridAOI();

    /** 根据mask判断两个实体是否interest */
    static inline bool is_interest(int32_t mask, const EntityCtx *ctx)
    {
        return mask & ctx->_mask;
    }

    /** 根据像素坐标判断是否在同一个格子内 */
    bool is_same_pos(int32_t x, int32_t y, int32_t dx, int32_t dy)
    {
        return (x / _pix_grid == dx / _pix_grid)
               && (y / _pix_grid == dy / _pix_grid);
    }

    bool set_visual_range(int32_t width, int32_t height);
    void set_size(int32_t width, int32_t height, int32_t pix_grid);

    struct EntityCtx *get_entity_ctx(EntityId id);
    /**
     * 获取某一范围内实体。底层这里只支持矩形，如果是其他形状的，上层根据实体位置再筛选即可
     */
    int32_t get_entity(EntityVector *list, int32_t srcx, int32_t srcy,
                        int32_t destx, int32_t desty);

    int32_t exit_entity(EntityId id, EntityVector *list = NULL);
    int32_t enter_entity(EntityId id, int32_t x, int32_t y, uint8_t mask, EntityVector *list = NULL);

    /**
     * 更新实体位置
     * @param list 接收实体更新的实体列表
     * @param list_in 接收实体进入的实体列表
     * @param list_out 接收实体消失的实体列表
     * @return <0错误，0正常，>0正常，但做了特殊处理
     */
    int32_t update_entity(EntityId id, int32_t x, int32_t y,
                          EntityVector *list = NULL, EntityVector *list_in = NULL,
                          EntityVector *list_out = NULL);

protected:
    void del_entity_vector(EntityVector *list)
    {
        // 太大的直接删除不要丢缓存，避免缓存消耗太多内存
        get_vector_pool()->destroy(list, list->size() > 128);
    }

    EntityVector *new_entity_vector()
    {
        EntityVector *vt = get_vector_pool()->construct();

        vt->clear();
        return vt;
    }

    void del_entity_ctx(struct EntityCtx *ctx)
    {
        del_entity_vector(ctx->_interest_me);

        get_ctx_pool()->destroy(ctx);
    }

    struct EntityCtx *new_entity_ctx()
    {
        struct EntityCtx *ctx = get_ctx_pool()->construct();

        ctx->_interest_me = new_entity_vector();

        return ctx;
    }

    /** 获取矩形内的实体 */
    int32_t get_range_entity(EntityVector *list, int32_t x, int32_t y,
                            int32_t dx, int32_t dy);

    // 获取视野范围
    void get_visual_range(int32_t &x, int32_t &y, int32_t &dx, int32_t &dy,
                          int32_t pos_x, int32_t pos_y);
private:
    static bool remove_entity_from_vector(EntityVector *list,
                                   const struct EntityCtx *ctx);
    /** 插入实体到格子内 */
    void insert_grid_entity(int32_t x, int32_t y, struct EntityCtx *ctx);
    /** 从格子列表内删除实体 */
    bool remove_grid_entity(int32_t x, int32_t y, const struct EntityCtx *ctx);
    /** 遍历矩形内的实体 */
    void each_range_entity(int32_t x, int32_t y, int32_t dx, int32_t dy,
                           std::function<void(EntityCtx *)> &&func);
    // 处理实体进入某个范围
    void entity_enter_range(struct EntityCtx *ctx, int32_t x, int32_t y,
                            int32_t dx, int32_t dy, EntityVector *list = NULL);
    // 处理实体退出某个范围
    void entity_exit_range(struct EntityCtx *ctx, int32_t x, int32_t y,
                           int32_t dx, int32_t dy, EntityVector *list = NULL);

    /** 校验格子坐标是否合法 */
    bool valid_pos(int32_t x, int32_t y, int32_t dx, int32_t dy) const
    {
        if (x < 0 || y < 0 || dx >= _width || dy >= _height)
        {
            ERROR("Invalid grid pos (%d, %d) (%d, %d), range (%d, %d)",
                  x, y, dx, dy, _width - 1, _height - 1);
            return false;
        }
        return true;
    }
private:
    using CtxPool          = ObjectPool<GridAOI::EntityCtx, 10240, 1024>;
    using EntityVectorPool = ObjectPool<GridAOI::EntityVector, 10240, 1024>;

    CtxPool *get_ctx_pool()
    {
        static thread_local CtxPool ctx_pool("grid_aoi_ctx");

        return &ctx_pool;
    }
    EntityVectorPool *get_vector_pool()
    {
        static thread_local EntityVectorPool ctx_pool("grid_aoi_vector");

        return &ctx_pool;
    }

protected:
    int32_t _width;    // 场景最大宽度(格子坐标)
    int32_t _height;   // 场景最大高度(格子坐标)
    int32_t _pix_grid; // 每个格子表示的像素大小

    // 格子数指以实体为中心，不包含当前格子，上下或者左右的格子数
    int32_t _visual_width;  // 视野宽度格子数
    int32_t _visual_height; // 视野高度格子数

    /**
     * 记录每个格子中的实体id列表
     * 有X[m][n]、X[_width * m + n]这两种存储方式，测试后发现第二种效率更高
     * https://stackoverflow.com/questions/936687/how-do-i-declare-a-2d-array-in-c-using-new
     */
    EntityVector *_entity_grid;

    /* 记录所有实体的数据 */
    EntitySet _entity_set;
};
