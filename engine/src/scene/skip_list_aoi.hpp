#pragma once

#include <list>
#include <functional>
#include "../pool/object_pool.hpp"

/**
 * 使用跳表实现的单链表AOI
 */
class SkipListAOI
{
public:
    class EntityCtx;
    using EntityId     = int64_t; // 用来标识实体的唯一id
    using EntityVector = std::vector<EntityCtx *>; // 实体列表
    using EntitySet    = std::unordered_map<EntityId, EntityCtx *>;

    /// 掩码，按位表示，第一位表示是否加入其他实体interest列表，其他由上层定义
    static const int INTEREST = 0x1;

    /// 链表中的节点基类
    class EntityCtx
    {
    public:
        void reset();
        bool operator<(const EntityCtx &other) const
        {
            // 需要保证索引节点(0 == _id)小于实体，这样坐标一样时索引必须在实体左边
            return _pos_x == other._pos_x ? (0 == _id) : _pos_x < other._pos_x;
        }
        bool operator>(const EntityCtx &other) const
        {
            // 需要保证索引节点(0 == _id)小于实体，这样坐标一样时索引必须在实体左边
            return _pos_x == other._pos_x ? (0 != _id) : _pos_x > other._pos_x;
        }

    public:
        /// 掩码，按位表示，第一位表示是否加入其他实体interest列表，其他由上层定义
        uint8_t _mask;
        int32_t _pos_x;  // 像素坐标x
        int32_t _pos_y;  // 像素坐标y
        int32_t _pos_z;  // 像素坐标z
        int32_t _visual; /// 视野大小(像素)
        EntityId _id;    /// 实体的唯一id，如玩家id(0表示索引)

        /**
         * 对我感兴趣的实体列表。比如我周围的玩家，需要看到我移动、放技能，都需要频繁广播给他们，
         * 可以直接从这个列表取。如果游戏并不是arpg，可能并不需要这个列表。
         * 当两个实体的mask存在交集时，将同时放入双方的_interest_me，但这只是做初步的筛选，例
         * 如不要把怪物放到玩家的_interest_me，但并不能处理玩家隐身等各种复杂的逻辑，还需要上层
         * 根据业务处理
         */
        EntityVector *_interest_me;

        // No iterators or references are invalidated
        // https://en.cppreference.com/w/cpp/container/list/insert
        /// 链表的迭代器可以存储，用于在链表中移动
        std::list<EntityCtx *>::iterator _iter;
    };

    /// 视野管理
    struct Visual
    {
        int32_t _visual;
        int32_t _counter;
    };

public:
    SkipListAOI();
    virtual ~SkipListAOI();

    SkipListAOI(const SkipListAOI &)  = delete;
    SkipListAOI(const SkipListAOI &&) = delete;
    SkipListAOI &operator=(const SkipListAOI &) = delete;
    SkipListAOI &operator=(const SkipListAOI &&) = delete;

    /// 打印整个链表，用于调试
    void dump() const;

    /// 遍历x轴链表上的实体(直到func返回false)
    void each_entity(std::function<bool(EntityCtx *)> &&func);

    /**
     * @brief 设置索引参数
     * @param index 索引大小
     * @param max_x 场景中x轴的最大值
     */
    void set_index(int32_t index, int32_t max_x);

    /**
     * @brief 实体进入场景
     * @param id 实体唯一id
     * @param x 像素坐标x
     * @param y 像素坐标y
     * @param z 像素坐标z
     * @param visual 视野
     * @param mask 掩码，由上层定义，影响interest_me列表
     * @param list_me_in 该列表中实体出现在我的视野范围
     * @param list_other_in 我出现在该列表中实体的视野范围内
     * @return
     */
    bool enter_entity(EntityId id, int32_t x, int32_t y, int32_t z,
                      int32_t visual, uint8_t mask,
                      EntityVector *list_me_in    = nullptr,
                      EntityVector *list_other_in = nullptr);

    /**
     * @brief 实体退出场景
     * @param id 实体唯一id
     * @param list interest_me实体列表
     * @return
     */
    int32_t exit_entity(EntityId id, EntityVector *list = nullptr);

    /**
     * 更新实体位置
     * @param list_me_in 该列表中实体出现在我的视野范围
     * @param list_other_in 我出现在该列表中实体的视野范围内
     * @param list_me_out 该列表中实体从我的视野范围消失
     * @param list_other_out 我从该列表中实体的视野范围内消失
     * @return <0错误，0正常，>0正常，但做了特殊处理
     */
    int32_t update_entity(EntityId id, int32_t x, int32_t y, int32_t z,
                          EntityVector *list_me_in     = nullptr,
                          EntityVector *list_other_in  = nullptr,
                          EntityVector *list_me_out    = nullptr,
                          EntityVector *list_other_out = nullptr);

    /**
     * @brief 更新实体的视野
     * @param id id 实体唯一id
     * @param visual 新视野大小
     * @param list_me_in 该列表中实体因视野扩大出现在我的视野范围
     * @param list_me_out 该列表中实体因视野缩小从我的视野范围消失
     */
    int32_t update_visual(EntityId id, int32_t visual, EntityVector *list_me_in,
                          EntityVector *list_me_out);

protected:
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
        get_vector_pool()->destroy(list, list->capacity() > 128);
    }

    EntityVector *new_entity_vector()
    {
        EntityVector *vt = get_vector_pool()->construct();

        vt->clear();
        return vt;
    }

    void del_entity_ctx(EntityCtx *ctx)
    {
        del_entity_vector(ctx->_interest_me);

        get_ctx_pool()->destroy(ctx);
    }

    EntityCtx *new_entity_ctx()
    {
        EntityCtx *ctx = get_ctx_pool()->construct();

        ctx->reset();
        ctx->_iter        = _list.end();
        ctx->_interest_me = new_entity_vector();

        return ctx;
    }

    /// 获取实体的指针
    EntityCtx *get_entity_ctx(EntityId id);

    void add_visual(int32_t visual);
    void del_visual(int32_t visual);

    /// 从无序数组中删除一个实体
    static bool remove_entity_from_vector(EntityVector *list,
                                          const EntityCtx *ctx);

    /// 判断点(x,y,z)是否在ctx视野范围内
    bool in_visual(const EntityCtx *ctx, int32_t x, int32_t y, int32_t z,
                   int32_t visual) const
    {
        // 假设视野是一个正方体，直接判断各坐标的距离而不是直接距离，这样运算比平方要快
        return visual > 0 && visual >= std::abs(ctx->_pos_x - x)
               && visual >= std::abs(ctx->_pos_z - z)
               && visual >= std::abs(ctx->_pos_y - y);
    }
    bool in_visual(const EntityCtx *ctx, const EntityCtx *other) const
    {
        return in_visual(ctx, other->_pos_x, other->_pos_y, other->_pos_z,
                         ctx->_visual);
    }

    /// 以ctx为中心，遍历指定范围内的实体
    void each_range_entity(const EntityCtx *ctx, int32_t visual,
                           std::function<void(EntityCtx *ctx)> &&func);
    /// 以ctx为中心，遍历指定范围内的实体
    void each_range_entity(const EntityCtx *ctx, int32_t prev_visual,
                           int32_t next_visual,
                           std::function<void(EntityCtx *ctx)> &&func);
    /// 实体other进入ctx的视野范围
    void on_enter_range(EntityCtx *ctx, EntityCtx *other, EntityVector *list_in,
                        bool me);
    /// 实体other退出ctx的视野范围
    void on_exit_range(EntityCtx *ctx, EntityCtx *other, EntityVector *list_out,
                       bool me);
    /// 实体在ctx的视野范围内变化
    void on_change_range(EntityCtx *ctx, EntityCtx *other, bool is_in,
                         bool was_in, EntityVector *list_in,
                         EntityVector *list_out, bool me);

protected:
    int32_t _max_visual;
    std::vector<Visual> _visual;

    EntitySet _entity_set; /// 记录所有实体的数据

    int32_t _index;                    /// 索引系数
    std::list<EntityCtx *> _list;      /// x轴链表
    std::vector<EntityCtx *> _indexer; /// 索引，用于在链表中快速跳转
};
