#pragma once

#include "../pool/object_pool.h"

/**
 * @brief 基于链表实现的三坐标AOI算法，支持单个实体可变视野
 */
class ListAOI
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

        int32_t _pos_x; // 像素坐标x
        int32_t _pos_y; // 像素坐标y
        int32_t _pos_z; // 像素坐标z
        int32_t _fov;   // 视野大小(Field of view,像素)
        EntityId _id;

        // 每个轴需要一个双链表
        EntityCtx *_next_x;
        EntityCtx *_prev_x;
        EntityCtx *_next_y;
        EntityCtx *_prev_y;
        EntityCtx *_next_z;
        EntityCtx *_prev_z;

        /**
         * 对我感兴趣的实体列表。比如我周围的玩家，需要看到我移动、放技能，都需要频繁广播给他们，
         * 可以直接从这个列表取。如果游戏并不是arpg，可能并不需要这个列表。
         * 当两个实体的mask存在交集时，将同时放入双方的_interest_me，但这只是做初步的筛选，例
         * 如不要把怪物放到玩家的_interest_me，但并不能处理玩家隐身等各种复杂的逻辑，还需要上层
         * 根据业务处理
         */
        EntityVector *_interest_me;
    };

    /// 记录视野相关信息
    struct FieldOfViewRef
    {
        int32_t _fov;
        int32_t _ref;
        FieldOfViewRef(int32_t fov, int32_t ref)
        {
            _fov = fov;
            _ref = ref;
        }
    };

public:
    ListAOI();
    virtual ~ListAOI();

    ListAOI(const ListAOI &)  = delete;
    ListAOI(const ListAOI &&) = delete;
    ListAOI &operator=(const ListAOI &) = delete;
    ListAOI &operator=(const ListAOI &&) = delete;

    /**
     * @brief 实体进入场景
     * @param id 实体唯一id
     * @param x 像素坐标x
     * @param y 像素坐标y
     * @param z 像素坐标z
     * @param fov 视野
     * @param mask 掩码，由上层定义，影响interest_me列表
     * @param list 在视野范围内的实体列表
     * @return
     */
    bool enter_entity(EntityId id, int32_t x, int32_t y, int32_t z, int32_t fov,
                      uint8_t mask, EntityVector *list = nullptr);

private:
    // 这些pool做成局部static变量以避免影响内存统计
    using CtxPool          = ObjectPool<EntityCtx, 10240, 1024>;
    using EntityVectorPool = ObjectPool<EntityVector, 10240, 1024>;

    CtxPool *get_ctx_pool()
    {
        static thread_local CtxPool ctx_pool("link_aoi_ctx");

        return &ctx_pool;
    }
    EntityVectorPool *get_vector_pool()
    {
        static thread_local EntityVectorPool ctx_pool("link_aoi_vector");

        return &ctx_pool;
    }

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

    /// 增加视野引用
    void add_fov(int32_t fov);
    /// 减少视野引用
    void dec_fov(int32_t fov);

private:
    // 每个轴需要一个双链表
    EntityCtx *_first_x;
    EntityCtx *_last_x;
    EntityCtx *_first_y;
    EntityCtx *_last_y;
    EntityCtx *_first_z;
    EntityCtx *_last_z;

    /**
     * 是否启用y轴
     * 如果不启用y轴，所有y坐标都传0，效果一样。但y轴链表的值都是一样的，每次遍历整个链表消耗比较大
     */
    bool _use_y;

    int32_t _max_fov;      /// 场景中最大的视野半径
    EntitySet _entity_set; /// 记录所有实体的数据

    /**
     * @brief 记录场景中所有的视野信息，用来维护_max_fvo
     * 如果每次_max_fov变化都需要遍历所有实体，消耗有点大。这个数组预计比较小，一般同一个场景中
     * 不同视野的实体都是个位数
     */
    std::vector<FieldOfViewRef> _fov_ref;
};
