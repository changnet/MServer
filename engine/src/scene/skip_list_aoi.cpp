#include <cmath>
#include "skip_list_aoi.hpp"

void SkipListAOI::EntityCtx::reset()
{
    mask_  = 0;
    pos_x_ = 0;
    pos_y_ = 0;
    pos_z_ = 0;

    id_          = 0;
    visual_      = 0;
    interest_me_ = nullptr;
}

SkipListAOI::SkipListAOI()
{
    index_      = 0;
    max_visual_ = 0;
}

SkipListAOI::~SkipListAOI()
{
    for (auto iter : indexer_) del_entity_ctx(iter);
    for (auto iter : entity_set_) del_entity_ctx(iter.second);
}

class SkipListAOI::EntityCtx *SkipListAOI::get_entity_ctx(EntityId id)
{
    EntitySet::const_iterator itr = entity_set_.find(id);
    if (entity_set_.end() == itr) return nullptr;

    return itr->second;
}

void SkipListAOI::add_visual(int32_t visual)
{
    assert(visual);

    for (auto &x : visual_)
    {
        if (x.visual_ == visual)
        {
            x.counter_++;
            return;
        }
    }

    visual_.emplace_back(Visual{visual, 1});
    if (visual > max_visual_) max_visual_ = visual;
}

void SkipListAOI::del_visual(int32_t visual)
{
    assert(visual);

    for (auto &x : visual_)
    {
        if (x.visual_ != visual) continue;

        x.counter_--;
        if (x.counter_ <= 0)
        {
            x = visual_.back();
            visual_.resize(visual_.size() - 1);
            if (visual == max_visual_)
            {
                max_visual_ = visual_.empty() ? 0 : visual_.back().visual_;
            }
        }
        return;
    }

    assert(false);
}

void SkipListAOI::set_index(int32_t index, int32_t max_x)
{
    // 这个只能在aoi初始化时设置一次
    assert(!index_ && index && max_x > 0);

    int32_t size = (int32_t)ceil(((double)max_x) / index);

    index_ = index;
    indexer_.reserve(size);

    // 在链表中构建特定的节点做为索引
    for (int32_t i = 0; i < size; i++)
    {
        EntityCtx *ctx = new_entity_ctx();

        list_.push_back(ctx);

        ctx->pos_x_ = i * index;
        ctx->iter_  = --list_.end();

        indexer_.push_back(ctx);
    }
}

void SkipListAOI::each_range_entity(const EntityCtx *ctx, int32_t prev_visual,
                                    int32_t next_visual,
                                    std::function<void(EntityCtx *ctx)> &&func)
{
    // 往链表左边遍历(注意这个for循环不会遍历begin本身，但由于第一个节点必须是索引，这里
    // 刚好不需要遍历)
    auto prev = ctx->iter_;
    for (--prev; prev != list_.begin() && (*prev)->pos_x_ >= prev_visual; --prev)
    {
        if ((*prev)->id_) func(*prev);
    }

    // 往链表右边遍历
    auto next = ctx->iter_;
    for (++next; next != list_.end() && (*next)->pos_x_ <= next_visual; ++next)
    {
        if ((*next)->id_) func(*next);
    }
}

void SkipListAOI::each_range_entity(const EntityCtx *ctx, int32_t visual,
                                    std::function<void(EntityCtx *)> &&func)
{
    each_range_entity(ctx, ctx->pos_x_ - visual, ctx->pos_x_ + visual,
                      std::forward<std::function<void(EntityCtx *)> &&>(func));
}

bool SkipListAOI::remove_entity_from_vector(EntityVector *list,
                                            const EntityCtx *ctx)
{
    for (auto &value : *list)
    {
        if (value == ctx)
        {
            // 用最后一个元素替换就好，不用移动其他元素
            value = list->back();
            list->pop_back();

            return true;
        }
    }

    return false;
}

void SkipListAOI::on_enter_range(EntityCtx *ctx, EntityCtx *other,
                                 EntityVector *list_in, bool me)
{
    // 这里只是表示进入一个轴，最终是否在视野范围内要判断三轴
    if (in_visual(ctx, other))
    {
        if (ctx->mask_ & INTEREST)
        {
            other->interest_me_->push_back(ctx);
        }

        // "别人进入我的视野" "我进入别人的视野" 取决于ctx与other的位置
        if (list_in) list_in->emplace_back(me ? other : ctx);
    }
}

void SkipListAOI::on_exit_range(EntityCtx *ctx, EntityCtx *other,
                                EntityVector *list_out, bool me)
{
    if (in_visual(ctx, other))
    {
        if (ctx->mask_ & INTEREST)
        {
            remove_entity_from_vector(other->interest_me_, ctx);
        }

        // "别人离开我的视野" "我离开别人的视野" 取决于ctx与other的位置
        if (list_out) list_out->emplace_back(me ? other : ctx);
    }
}

void SkipListAOI::insert_entity(EntityCtx *ctx)
{
    // 插入链表
    assert(index_ && indexer_.size() > size_t(ctx->pos_x_ / index_));

    auto iter = indexer_[ctx->pos_x_ / index_]->iter_;
    while (iter != list_.end() && **iter < *ctx) ++iter;

    // 在iter之前插入当前实体
    ctx->iter_ = list_.insert(iter, ctx);
}

bool SkipListAOI::enter_entity(EntityId id, int32_t x, int32_t y, int32_t z,
                               int32_t visual, uint8_t mask,
                               EntityVector *list_me_in,
                               EntityVector *list_other_in)
{
    // 防止重复进入场景
    auto ret = entity_set_.emplace(id, nullptr);
    if (false == ret.second)
    {
        ELOG("%s entity already in scene " FMT64d, __FUNCTION__, id);
        return false;
    }

    EntityCtx *ctx    = new_entity_ctx();
    ret.first->second = ctx;

    ctx->id_     = id;
    ctx->mask_   = mask;
    ctx->pos_x_  = x;
    ctx->pos_y_  = y;
    ctx->pos_z_  = z;
    ctx->visual_ = visual;

    if (visual && (mask & INTEREST))
    {
        add_visual(visual);
    }

    insert_entity(ctx);
    each_range_entity(ctx, max_visual_,
                      [this, ctx, list_me_in, list_other_in](EntityCtx *other)
                      {
                          // 别人出现在自己的视野
                          on_enter_range(ctx, other, list_me_in, true);
                          // 自己出现在别人的视野
                          on_enter_range(other, ctx, list_other_in, false);
                      });

    return true;
}

int32_t SkipListAOI::exit_entity(EntityId id, EntityVector *list)
{
    EntitySet::iterator iter = entity_set_.find(id);
    if (entity_set_.end() == iter) return 1;

    EntityCtx *ctx = iter->second;
    entity_set_.erase(iter);

    // 从别人的interest_me列表删除自己
    if (ctx->visual_ > 0 && ctx->mask_ & INTEREST)
    {
        each_range_entity(ctx, ctx->visual_,
                          [this, ctx](EntityCtx *other)
                          { on_exit_range(ctx, other, nullptr, true); });
    }

    // 从链表中删除自己
    list_.erase(ctx->iter_);

    // 是否需要返回关注自己离开场景的实体列表
    if (list)
    {
        EntityVector *interest_me = ctx->interest_me_;
        list->insert(list->end(), interest_me->begin(), interest_me->end());
    }

    del_entity_ctx(ctx);

    return 0;
}

void SkipListAOI::on_change_range(EntityCtx *ctx, EntityCtx *other, bool is_in,
                                  bool was_in, EntityVector *list_in,
                                  EntityVector *list_out, bool me)
{
    if (is_in && !was_in)
    {
        // 进入我视野
        other->interest_me_->emplace_back(ctx);
        if (list_in) list_in->emplace_back(me ? other : ctx);
    }
    else if (!is_in && was_in)
    {
        // 离开我视野
        remove_entity_from_vector(other->interest_me_, ctx);
        if (list_out) list_out->emplace_back(me ? other : ctx);
    }
}

void SkipListAOI::shift_entity(EntityCtx *ctx, int32_t old_x)
{
    bool update = false;
    int32_t x   = ctx->pos_x_;
    auto iter   = ctx->iter_;
    if (x > old_x)
    {

        ++iter;
        while (iter != list_.end() && **iter < *ctx)
        {
            ++iter;
            update = true;
        }
    }
    else if (x < old_x)
    {
        --iter;
        while (iter != list_.begin() && **iter > *ctx)
        {
            --iter;
            update = true;
        }
        if (update) ++iter; // iter不会指向list_.begin，也肯定不会指向它
    }

    // 在iter之前插入当前实体
    if (update)
    {
        list_.erase(ctx->iter_);
        ctx->iter_ = list_.insert(iter, ctx);
    }
}

int32_t SkipListAOI::update_entity_short(EntityCtx *ctx, int32_t old_x,
                                         int32_t old_y, int32_t old_z,
                                         EntityVector *list_me_in,
                                         EntityVector *list_other_in,
                                         EntityVector *list_me_out,
                                         EntityVector *list_other_out)
{
    int32_t x           = ctx->pos_x_;
    int32_t prev_visual = 0;
    int32_t next_visual = 0;
    if (x > old_x)
    {
        next_visual = x + max_visual_;
        prev_visual = old_x - max_visual_;
    }
    else
    {
        prev_visual = x - max_visual_;
        next_visual = old_x + max_visual_;
    }

    shift_entity(ctx, old_x);

    each_range_entity(
        ctx, prev_visual, next_visual,
        [this, ctx, old_x, old_y, old_z, list_me_in, list_other_in, list_me_out,
         list_other_out](EntityCtx *other)
        {
            // 只有我对目标interest，对方才会在我视野范围内变化
            if (ctx->visual_ && ctx->mask_ & INTEREST)
            {
                bool is_in_me = in_visual(ctx, other);
                bool was_in_me =
                    in_visual(other, old_x, old_y, old_z, ctx->visual_);
                on_change_range(ctx, other, is_in_me, was_in_me, list_me_in,
                                list_me_out, true);
            }

            // 只有目标对我interest，我才会在对方视野范围内变化
            if (other->visual_ && other->mask_ & INTEREST)
            {
                bool is_in_other = in_visual(other, ctx);
                bool was_in_other =
                    in_visual(other, old_x, old_y, old_z, other->visual_);
                on_change_range(other, ctx, is_in_other, was_in_other,
                                list_other_in, list_other_out, false);
            }
        });

    return 0;
}

int32_t SkipListAOI::update_entity_long(EntityCtx *ctx, int32_t old_x,
                                        int32_t old_y, int32_t old_z,
                                        EntityVector *list_me_in,
                                        EntityVector *list_other_in,
                                        EntityVector *list_me_out,
                                        EntityVector *list_other_out)
{
    // 遍历旧视野，这个范围内的实体现在肯定不在新位置的视野范围内(is_in_xxx必定为false)
    each_range_entity(
        ctx, old_x - max_visual_, old_x + max_visual_,
        [this, ctx, old_x, old_y, old_z, list_me_in, list_other_in, list_me_out,
         list_other_out](EntityCtx *other)
        {
            // 只有我对目标interest，对方才会在我视野范围内变化
            if (ctx->visual_ && ctx->mask_ & INTEREST)
            {
                bool was_in_me =
                    in_visual(other, old_x, old_y, old_z, ctx->visual_);
                on_change_range(ctx, other, false, was_in_me, list_me_in,
                                list_me_out, true);
            }

            // 只有目标对我interest，我才会在对方视野范围内变化
            if (other->visual_ && other->mask_ & INTEREST)
            {
                bool was_in_other =
                    in_visual(other, old_x, old_y, old_z, other->visual_);
                on_change_range(other, ctx, false, was_in_other, list_other_in,
                                list_other_out, false);
            }
        });
    /*
     * 移动实体到链表中的新位置
     * TODO 很难判断是使用insert还是shift快一些，现在默认两位置至少跨一个index范围才使用
     * 索引插入的方式
     */
    int32_t old_index = old_x / index_;
    int32_t new_index = ctx->pos_x_ / index_;
    if (std::abs(new_index - old_index) > 1)
    {
        list_.erase(ctx->iter_);
        insert_entity(ctx);
    }
    else
    {
        shift_entity(ctx, old_x);
    }
    // 遍历新视野，这个范围内的实体现在肯定不在新位置的视野范围内(was_in_xxx必定为false)
    each_range_entity(ctx, max_visual_,
                      [this, ctx, list_me_in, list_other_in, list_me_out,
                       list_other_out](EntityCtx *other)
                      {
                          // 只有我对目标interest，对方才会在我视野范围内变化
                          if (ctx->visual_ && ctx->mask_ & INTEREST)
                          {
                              bool is_in_me = in_visual(ctx, other);
                              on_change_range(ctx, other, is_in_me, false,
                                              list_me_in, list_me_out, true);
                          }

                          // 只有目标对我interest，我才会在对方视野范围内变化
                          if (other->visual_ && other->mask_ & INTEREST)
                          {
                              bool is_in_other = in_visual(other, ctx);
                              on_change_range(other, ctx, is_in_other, false,
                                              list_other_in, list_other_out,
                                              false);
                          }
                      });
    return 0;
}

int32_t SkipListAOI::update_entity(EntityId id, int32_t x, int32_t y, int32_t z,
                                   EntityVector *list_me_in,
                                   EntityVector *list_other_in,
                                   EntityVector *list_me_out,
                                   EntityVector *list_other_out)
{
    EntityCtx *ctx = get_entity_ctx(id);
    if (!ctx)
    {
        ELOG("%s no ctx found: " FMT64d, __FUNCTION__, id);
        return -1;
    }

    int32_t old_x = ctx->pos_x_;
    int32_t old_y = ctx->pos_y_;
    int32_t old_z = ctx->pos_z_;

    ctx->pos_x_ = x;
    ctx->pos_y_ = y;
    ctx->pos_z_ = z;

    /**
     * 如果移动距离比较小，两个位置的视野有重叠，那么遍历两个视野的并集即可
     * 如果移动距离比较大，两个位置的视野有重叠，那么分开遍历，避免遍历两个视野之间无效的实体
     */
    return (std::abs(x - old_x) > 2 * max_visual_)
               ? update_entity_long(ctx, old_x, old_y, old_z, list_me_in,
                                    list_other_in, list_me_out, list_other_out)
               : update_entity_short(ctx, old_x, old_y, old_z, list_me_in,
                                     list_other_in, list_me_out, list_other_out);
}

int32_t SkipListAOI::update_visual(EntityId id, int32_t visual,
                                   EntityVector *list_me_in,
                                   EntityVector *list_me_out)
{
    EntityCtx *ctx = get_entity_ctx(id);
    if (!ctx)
    {
        ELOG("%s no ctx found: " FMT64d, __FUNCTION__, id);
        return -1;
    }
    if (visual == ctx->visual_) return 0;
    assert(ctx->mask_ & INTEREST);

    int32_t old_visual = ctx->visual_;

    if (old_visual) del_visual(old_visual);
    ctx->visual_ = visual;
    if (visual) add_visual(visual);

    // 不用取max_visual_，因为只是自己的视野变化了，与其他人无关
    int32_t max_visual  = std::max(old_visual, visual);
    int32_t prev_visual = ctx->pos_x_ - max_visual;
    int32_t next_visual = ctx->pos_x_ + max_visual;
    each_range_entity(
        ctx, prev_visual, next_visual,
        [this, ctx, old_visual, list_me_in, list_me_out](EntityCtx *other)
        {
            bool is_in_me  = in_visual(ctx, other);
            bool was_in_me = in_visual(ctx, other->pos_x_, other->pos_y_,
                                       other->pos_z_, old_visual);
            on_change_range(ctx, other, is_in_me, was_in_me, list_me_in,
                            list_me_out, true);
        });

    return 0;
}

void SkipListAOI::each_entity(std::function<bool(EntityCtx *)> &&func)
{
    for (auto x : list_)
    {
        if (x->id_) func(x);
    }
}

bool SkipListAOI::valid_dump(bool dump) const
{
#define DUMP_PRINTF(...) \
    if (dump) PLOG(__VA_ARGS__)

    bool ok         = true;
    EntityCtx *last = nullptr;
    DUMP_PRINTF("================ >>");
    for (auto x : list_)
    {
        DUMP_PRINTF("id = " FMT64d ", x = %5d, y = %5d, z = %5d", x->id_,
                    x->pos_x_, x->pos_y_, x->pos_z_);
        if (last && *last > *x)
        {
            DUMP_PRINTF("dump error");
            ok = false;
        }
        last = x;
    }
    DUMP_PRINTF("================ <<");

#undef DUMP_PRINTF

    return ok;
}
