#pragma once

#include <functional>
#include "pool/cache_pool.hpp"

/**
 * @brief 基于十字链表(Orthogonal List)实现的三坐标AOI算法，支持单个实体可变视野
 */
class OrthListAOI
{
public:
    class EntityCtx;
    using EntityId     = int64_t; // 用来标识实体的唯一id
    using EntityVector = std::vector<EntityCtx *>; // 实体列表
    using EntitySet    = std::unordered_map<EntityId, EntityCtx *>;

    /// 掩码，按位表示，第一位表示是否加入其他实体interest列表，其他由上层定义
    static const int INTEREST = 0x1;

    /// 链表中节点类型
    enum CtxType
    {
        CT_VISUAL_PREV = -1, /// 视野左边界
        CT_ENTITY      = 0,  /// 实体本身
        CT_VISUAL_NEXT = 1   /// 视野右边界
    };

    /// 链表中的节点基类
    class Ctx
    {
    public:
        virtual void reset();
        virtual int32_t type() const = 0;
        virtual EntityCtx *entity() { return nullptr; }

        /// 更新坐标
        void update_pos(int32_t x, int32_t y, int32_t z);
        void update_old_pos();

        /**
         * 对比两个节点的位置大小
         * @return int32_t 左边<0，等于0，右边>0
         */
        template <int32_t Ctx::*_pos> int32_t comp(const Ctx *other) const
        {
            if (this->*_pos > other->*_pos)
            {
                return 1;
            }
            else if (this->*_pos == other->*_pos)
            {
                // 当坐标一致时，不同类型的节点在链表中有特定的位置，必须按
                // 左边界>>>实体>>>右边界 这个顺序排列，不然移动视野边界的时候就可能会漏掉一些实体
                return type() - other->type();
            }
            return -1;
        }

    public:
        int32_t _pos_x; // 像素坐标x
        int32_t _pos_y; // 像素坐标y
        int32_t _pos_z; // 像素坐标z
        int32_t _old_x; // 旧像素坐标x
        int32_t _old_y; // 旧像素坐标y
        int32_t _old_z; // 旧像素坐标z

        // 每个轴需要一个双链表
        Ctx *_next_x;
        Ctx *_prev_x;
        Ctx *_next_y;
        Ctx *_prev_y;
        Ctx *_next_z;
        Ctx *_prev_z;
    };

    /// 实体的视野左右边界
    template <CtxType _type> class VisualCtx final : public Ctx
    {
    public:
        explicit VisualCtx(EntityCtx *ctx)
        {
            Ctx::reset();
            _entity = ctx;
        }
        int32_t type() const override { return _type; }
        EntityCtx *entity() override { return _entity; }

    private:
        EntityCtx *_entity; /// 该视野边界所属的实体
    };

    /// 场景中单个实体的类型、坐标等数据
    class EntityCtx final : public Ctx
    {
    public:
        explicit EntityCtx() : _next_v(this), _prev_v(this) {}
        int32_t type() const override { return CT_ENTITY; }

        /// 这个实体是否拥有视野(一些怪物之类的不需要视野，可提高aoi的效率，需要附近的实体
        /// 列表可通过get_visual_entity实时获取)
        bool has_visual() const { return _visual > 0 && (_mask & INTEREST); }

        void reset() override;
        /// 更新视野坐标
        void update_visual(int32_t visual);

    public:
        /// 掩码，按位表示，第一位表示是否加入其他实体interest列表，其他由上层定义
        uint8_t _mask;
        uint32_t _mark;      /// 计数器，用于标记是否重复
        int32_t _visual;     /// 视野大小(像素)
        int32_t _old_visual; /// 旧的视野大小
        EntityId _id;        /// 实体的唯一id，如玩家id

        /**
         * 对我感兴趣的实体列表。比如我周围的玩家，需要看到我移动、放技能，都需要频繁广播给他们，
         * 可以直接从这个列表取。如果游戏并不是arpg，可能并不需要这个列表。
         * 当两个实体的mask存在交集时，将同时放入双方的_interest_me，但这只是做初步的筛选，例
         * 如不要把怪物放到玩家的_interest_me，但并不能处理玩家隐身等各种复杂的逻辑，还需要上层
         * 根据业务处理
         */
        EntityVector *_interest_me;

        VisualCtx<CT_VISUAL_NEXT> _next_v; /// 视野的左边界节点
        VisualCtx<CT_VISUAL_PREV> _prev_v; /// 视野的右边界节点
    };

public:
    OrthListAOI();
    virtual ~OrthListAOI();

    OrthListAOI(const OrthListAOI &)  = delete;
    OrthListAOI(const OrthListAOI &&) = delete;
    OrthListAOI &operator=(const OrthListAOI &) = delete;
    OrthListAOI &operator=(const OrthListAOI &&) = delete;

    /// 获取实体的指针
    EntityCtx *get_entity_ctx(EntityId id);

    /// 遍历x轴链表上的实体(直到func返回false)
    void each_entity(std::function<bool(EntityCtx *)> &&func);

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
    using CtxPool          = CachePool<EntityCtx, 10240, 1024>;
    using EntityVectorPool = CachePool<EntityVector, 10240, 1024>;

    /// 从无序数组中删除一个实体
    static bool remove_entity_from_vector(EntityVector *list,
                                          const EntityCtx *ctx);

    /// 以ctx为中心，遍历指定范围内的实体
    void each_range_entity(const Ctx *ctx, int32_t visual,
                           std::function<void(EntityCtx *ctx)> &&func);

    /// 实体other进入ctx的视野范围
    void on_enter_range(EntityCtx *ctx, EntityCtx *other, EntityVector *list_in,
                        bool me);
    /// 实体other退出ctx的视野范围
    void on_exit_range(EntityCtx *ctx, EntityCtx *other, EntityVector *list_out,
                       bool me);
    /// 实体other退出ctx的旧的视野范围
    void on_exit_old_range(EntityCtx *ctx, EntityCtx *other,
                           EntityVector *list_out, bool me, int32_t visual = -1);

    /// 当other的位置有变化，离开ctx的视野，则只需要使用other的旧坐标，但视野是用的ctx的
    void on_exit_old_pos_range(EntityCtx *ctx, EntityCtx *other,
                               EntityVector *list_out, bool me);

    bool valid_dump(bool dump) const; /// 打印整个链表，用于调试

    CtxPool *get_ctx_pool()
    {
        static thread_local CtxPool ctx_pool("orth_link_aoi_ctx");

        return &ctx_pool;
    }
    EntityVectorPool *get_vector_pool()
    {
        static thread_local EntityVectorPool ctx_pool("orth_link_aoi_vector");

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
        ctx->_interest_me = new_entity_vector();

        return ctx;
    }

    /// 是否已经被标记
    bool had_mark(const EntityCtx *ctx) const
    {
        // > _mark表示在当前这次操作中被标记过
        // == _now_mark表示是在同一个轴标记(例如移动x轴上的左边界、实体、右边界会遇到同一个
        // 实体，但处理逻辑是不一样的，不会被认为是重复)
        return (ctx->_mark >= _mark) && (ctx->_mark != _now_mark);
    }

    /// 判断点(x,y,z)是否在ctx视野范围内
    bool in_visual(const EntityCtx *ctx, int32_t x, int32_t y, int32_t z,
                   int32_t visual = -1) const
    {
        // 假设视野是一个正方体，直接判断各坐标的距离而不是直接距离，这样运算比平方要快
        if (-1 == visual) visual = ctx->_visual;
        return visual > 0 && visual >= std::abs(ctx->_pos_x - x)
               && visual >= std::abs(ctx->_pos_z - z)
               && (!_use_y || visual >= std::abs(ctx->_pos_y - y));
    }

    /// 把ctx插入到链表合适的地方
    template <int32_t Ctx::*_pos, Ctx *Ctx::*_next, Ctx *Ctx::*_prev>
    void insert_list(Ctx *&list, Ctx *ctx, std::function<void(Ctx *ctx)> &&func);

    /// 把ctx插入到链表合适的地方
    template <int32_t Ctx::*_pos, Ctx *Ctx::*_next, Ctx *Ctx::*_prev>
    void insert_entity(Ctx *&list, EntityCtx *ctx,
                       std::function<void(Ctx *ctx)> &&func);

    /// 把ctx的视野边界插入到链表合适的地方
    template <int32_t Ctx::*_pos, Ctx *Ctx::*_next, Ctx *Ctx::*_prev>
    void insert_visual_list(Ctx *&list, EntityCtx *ctx,
                            std::function<void(Ctx *ctx)> &&func);

    /// 把ctx从链表中删除
    template <Ctx *Ctx::*_next, Ctx *Ctx::*_prev>
    void remove_list(Ctx *&list, Ctx *ctx);

    /// 把ctx从链表中删除
    template <Ctx *Ctx::*_next, Ctx *Ctx::*_prev>
    void remove_entity(Ctx *&list, EntityCtx *ctx);

    /// 把ctx移动到链表合适的地方
    template <int32_t Ctx::*_new, int32_t Ctx::*_old, Ctx *Ctx::*_next, Ctx *Ctx::*_prev>
    void shift_entity(Ctx *&list, EntityCtx *ctx, EntityVector *list_me_in,
                      EntityVector *list_other_in, EntityVector *list_me_out,
                      EntityVector *list_other_out);

    /**
     * 把ctx的视野边界移动到链表合适的地方
     * @param list_me_in 该列表中实体出现在我的视野范围
     * @param list_me_out 该列表中实体从我的视野范围消失
     */
    template <int32_t Ctx::*_pos, Ctx *Ctx::*_next, Ctx *Ctx::*_prev>
    void shift_visual(Ctx *&list, EntityCtx *ctx, EntityVector *list_me_in,
                      EntityVector *list_me_out, int32_t shift_type);

    /// 向右移动到链表合适的地方
    template <int32_t Ctx::*_pos, Ctx *Ctx::*_next, Ctx *Ctx::*_prev>
    void shift_list_next(Ctx *&list, Ctx *ctx, EntityVector *list_me_in,
                         EntityVector *list_other_in, EntityVector *list_me_out,
                         EntityVector *list_other_out);
    /// 向左移动到链表合适的地方
    template <int32_t Ctx::*_pos, Ctx *Ctx::*_next, Ctx *Ctx::*_prev>
    void shift_list_prev(Ctx *&list, Ctx *ctx, EntityVector *list_me_in,
                         EntityVector *list_other_in, EntityVector *list_me_out,
                         EntityVector *list_other_out);
    /// 移动视野边界时，检测其他实体在视野中的变化
    void on_shift_visual(Ctx *ctx, Ctx *other, EntityVector *list_me_in,
                         EntityVector *list_me_out, int32_t shift_type);
    /// 移动实体时，检测自己在其他实体视野中的变化
    void on_shift_entity(EntityCtx *ctx, Ctx *other, EntityVector *list_other_in,
                         EntityVector *list_other_out, int32_t shift_type);

    /// 把ctx的视野边界节点插入链表
    int32_t insert_visual(EntityCtx *ctx, EntityVector *list_in);
    /// 把ctx的视野边界节点移出链表
    int32_t remove_visual(EntityCtx *ctx, EntityVector *list_out);

    /// 更新mark
    void update_mark();

protected:
    /**
     * 是否启用y轴
     * 如果不启用y轴，避免对y轴链表进行插入、删除操作
     */
    bool _use_y;

    uint32_t _mark; /// 标记，用于标记同一个实体是否被处理过
    uint32_t _now_mark; /// 当前使用的标记，用于区分在哪条轴使用

    // 每个轴需要一个双向链表
    Ctx *_first_x;
    Ctx *_first_y;
    Ctx *_first_z;

    EntitySet _entity_set; /// 记录所有实体的数据
};
