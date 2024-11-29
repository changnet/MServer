#include "orth_list_aoi.hpp"

// /////////////////////////////////////////////////////////////////////////////

void OrthListAOI::Ctx::reset()
{
    pos_x_ = 0;
    pos_y_ = 0;
    pos_z_ = 0;
    old_x_ = 0;
    old_y_ = 0;
    old_z_ = 0;

    next_x_ = nullptr;
    prev_x_ = nullptr;
    next_y_ = nullptr;
    prev_y_ = nullptr;
    next_z_ = nullptr;
    prev_z_ = nullptr;
}

void OrthListAOI::Ctx::update_old_pos()
{
    old_x_ = pos_x_;
    old_y_ = pos_y_;
    old_z_ = pos_z_;
}

void OrthListAOI::Ctx::update_pos(int32_t x, int32_t y, int32_t z)
{
    update_old_pos();

    pos_x_ = x;
    pos_y_ = y;
    pos_z_ = z;
}

void OrthListAOI::EntityCtx::reset()
{
    Ctx::reset();

    mask_        = 0;
    id_          = 0;
    mark_        = 0;
    visual_      = 0;
    old_visual_  = 0;
    interest_me_ = nullptr;

    next_v_.reset();
    prev_v_.reset();
}

void OrthListAOI::EntityCtx::update_visual(int32_t visual)
{
    old_visual_ = visual_;
    visual_     = visual;

    prev_v_.update_pos(pos_x_ - visual_, pos_y_ - visual_, pos_z_ - visual_);
    next_v_.update_pos(pos_x_ + visual_, pos_y_ + visual_, pos_z_ + visual_);
}

// /////////////////////////////////////////////////////////////////////////////

OrthListAOI::OrthListAOI()
{
    first_x_ = nullptr;
    first_y_ = nullptr;
    first_z_ = nullptr;

    use_y_    = true;
    mark_     = 0;
    now_mark_ = 0;
}

OrthListAOI::~OrthListAOI()
{
    for (auto &iter : entity_set_) del_entity_ctx(iter.second);
}

// 获取实体的ctx
OrthListAOI::EntityCtx *OrthListAOI::get_entity_ctx(EntityId id)
{
    EntitySet::const_iterator itr = entity_set_.find(id);
    if (entity_set_.end() == itr) return nullptr;

    return itr->second;
}

bool OrthListAOI::remove_entity_from_vector(EntityVector *list,
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

void OrthListAOI::each_range_entity(const Ctx *ctx, int32_t visual,
                                    std::function<void(EntityCtx *)> &&func)
{
    // 实体同时存在三轴链表上，只需要遍历其中一个链表即可

    // 往链表左边遍历
    int32_t prev_visual = ctx->pos_x_ - visual;
    Ctx *prev           = ctx->prev_x_;
    while (prev && prev->pos_x_ >= prev_visual)
    {
        if (CT_ENTITY == prev->type()) func((EntityCtx *)prev);
        prev = prev->prev_x_;
    }

    // 往链表右边遍历
    int32_t next_visual = ctx->pos_x_ + visual;
    Ctx *next           = ctx->next_x_;
    while (next && next->pos_x_ <= next_visual)
    {
        if (CT_ENTITY == next->type()) func((EntityCtx *)next);
        next = next->next_x_;
    }
}

void OrthListAOI::on_enter_range(EntityCtx *ctx, EntityCtx *other,
                                 EntityVector *list_in, bool me)
{
    // 这里只是表示进入一个轴，最终是否在视野范围内要判断三轴
    if (in_visual(ctx, other->pos_x_, other->pos_y_, other->pos_z_))
    {
        if (ctx->mask_ & INTEREST)
        {
            other->interest_me_->push_back(ctx);
        }

        // "别人进入我的视野" "我进入别人的视野" 取决于ctx与other的位置
        if (list_in) list_in->emplace_back(me ? other : ctx);
    }
}

void OrthListAOI::on_exit_range(EntityCtx *ctx, EntityCtx *other,
                                EntityVector *list_out, bool me)
{
    if (in_visual(ctx, other->pos_x_, other->pos_y_, other->pos_z_))
    {
        if (ctx->mask_ & INTEREST)
        {
            remove_entity_from_vector(other->interest_me_, ctx);
        }

        // "别人离开我的视野" "我离开别人的视野" 取决于ctx与other的位置
        if (list_out) list_out->emplace_back(me ? other : ctx);
    }
}

void OrthListAOI::on_exit_old_range(EntityCtx *ctx, EntityCtx *other,
                                    EntityVector *list_out, bool me,
                                    int32_t visual)
{
    // 判断旧视野
    // 当ctx的视野有变化（视野缩小）则需要使用ctx的旧坐标、旧视野来判断
    // 当ctx的位置有变化，则只需要使用ctx的旧坐标，但视野是用的other的
    if (-1 == visual) visual = ctx->old_visual_;
    if (in_visual(other, ctx->old_x_, ctx->old_y_, ctx->old_z_, visual))
    {
        if (ctx->mask_ & INTEREST)
        {
            remove_entity_from_vector(other->interest_me_, ctx);
        }

        // "别人离开我的视野" "我离开别人的视野" 取决于ctx与other的位置
        if (list_out) list_out->emplace_back(me ? other : ctx);
    }
}

void OrthListAOI::on_exit_old_pos_range(EntityCtx *ctx, EntityCtx *other,
                                        EntityVector *list_out, bool me)
{

    if (in_visual(ctx, other->old_x_, other->old_y_, other->old_z_))
    {
        if (ctx->mask_ & INTEREST)
        {
            remove_entity_from_vector(other->interest_me_, ctx);
        }

        // "别人离开我的视野" "我离开别人的视野" 取决于ctx与other的位置
        if (list_out) list_out->emplace_back(me ? other : ctx);
    }
}

bool OrthListAOI::enter_entity(EntityId id, int32_t x, int32_t y, int32_t z,
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

    ctx->id_   = id;
    ctx->mask_ = mask;
    ctx->update_pos(x, y, z);
    ctx->update_visual(visual);

    update_mark();

    insert_entity<&Ctx::pos_x_, &Ctx::next_x_, &Ctx::prev_x_>(
        first_x_, ctx,
        [this, ctx, list_me_in, list_other_in](Ctx *other)
        {
            if (CT_ENTITY == other->type())
            {
                // 对方进入我的视野范围
                if (ctx != other)
                {
                    on_enter_range(ctx, (EntityCtx *)other, list_me_in, true);
                }
            }
            else
            {
                // 我进入对方视野范围
                EntityCtx *entity = other->entity();
                if (ctx != entity && !had_mark(entity))
                {
                    // enter的时候，可能会跨越对方左右视野边界，这时候用now_mark_ + 1
                    // 来标记该实体已处理过。(虽然跨越对方两边视野边界时一定不会进入对方视野，
                    // 不影响逻辑，但加个标记避免二次视野判断，应该可以提高效率)
                    entity->mark_ = now_mark_ + 1;
                    on_enter_range(entity, ctx, list_other_in, false);
                }
            }
        });
    // 插入另外的二轴，由于x轴已经遍历了，所以这里不需要再处理视野问题
    if (use_y_)
    {
        insert_entity<&Ctx::pos_y_, &Ctx::next_y_, &Ctx::prev_y_>(first_y_, ctx,
                                                                  nullptr);
    }
    insert_entity<&Ctx::pos_z_, &Ctx::next_z_, &Ctx::prev_z_>(first_z_, ctx,
                                                              nullptr);

    return true;
}

int32_t OrthListAOI::exit_entity(EntityId id, EntityVector *list)
{
    EntitySet::iterator iter = entity_set_.find(id);
    if (entity_set_.end() == iter) return 1;

    EntityCtx *ctx = iter->second;
    entity_set_.erase(iter);

    // 从别人的interest_me列表删除自己
    if (ctx->has_visual())
    {
        each_range_entity(ctx, ctx->visual_,
                          [this, ctx](EntityCtx *other)
                          { on_exit_range(ctx, other, nullptr, true); });
    }

    // 从三轴中删除自己
    remove_entity<&Ctx::next_x_, &Ctx::prev_x_>(first_x_, ctx);
    if (use_y_)
    {
        remove_entity<&Ctx::next_y_, &Ctx::prev_y_>(first_y_, ctx);
    }
    remove_entity<&Ctx::next_z_, &Ctx::prev_z_>(first_z_, ctx);

    // 是否需要返回关注自己离开场景的实体列表
    if (list)
    {
        EntityVector *interest_me = ctx->interest_me_;
        list->insert(list->end(), interest_me->begin(), interest_me->end());
    }

    del_entity_ctx(ctx);

    return 0;
}

void OrthListAOI::update_mark()
{
    /**
     * 在三轴中移动时，遍历的实体可能会出现重复，有几种方式去重
     * 1. 用std::map或者std::unordered_map去重
     * 2. 遍历数组去重
     * 3. 通过判断坐标，是否重复在其他轴出现过
     * 4. 在每个实体上加一个计数器，每次遍历到就加1，通过计数器判断是否重复(计数器重置则需要
     *    重置所有实体的计数器，避免重复
     */
    if (now_mark_ >= 0xFFFFFFFF - 3)
    {
        now_mark_ = 0;
        for (auto iter = entity_set_.begin(); iter != entity_set_.end(); iter++)
        {
            iter->second->mark_ = 0;
        }
    }

    // 最多有3轴，每轴使用一个标记
    now_mark_ += 3;
    mark_ = now_mark_;
}

int32_t OrthListAOI::update_entity(EntityId id, int32_t x, int32_t y, int32_t z,
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

    ctx->update_pos(x, y, z); // 先设置新坐标
    // 更新视野坐标，即使视野没变化，也需要更新old_visual，因为on_exit_old_range用到
    ctx->update_visual(ctx->visual_);

    update_mark();

    shift_entity<&Ctx::pos_x_, &Ctx::old_x_, &Ctx::next_x_, &Ctx::prev_x_>(
        first_x_, ctx, list_me_in, list_other_in, list_me_out, list_other_out);

    if (use_y_)
    {
        shift_entity<&Ctx::pos_y_, &Ctx::old_y_, &Ctx::next_y_, &Ctx::prev_y_>(
            first_y_, ctx, list_me_in, list_other_in, list_me_out,
            list_other_out);
    }

    shift_entity<&Ctx::pos_z_, &Ctx::old_z_, &Ctx::next_z_, &Ctx::prev_z_>(
        first_z_, ctx, list_me_in, list_other_in, list_me_out, list_other_out);

    return 0;
}

void OrthListAOI::on_shift_visual(Ctx *ctx, Ctx *other, EntityVector *list_me_in,
                                  EntityVector *list_me_out, int32_t shift_type)
{
    int32_t type = other->type();
    if (CT_ENTITY != type || had_mark((EntityCtx *)other)) return;

    ((EntityCtx *)other)->mark_ = now_mark_;

    EntityCtx *entity = ctx->entity();

    // 视野边界移动时是按顺序先右中左，或者左中右移动的，不应该会跨过自己本身的实体
    assert(entity != other);

    // 向右移动右边界或者向左移动左边界，检测其他实体进入视野。其他情况检测退出视野
    shift_type == ctx->type()
        ? on_enter_range(entity, (EntityCtx *)other, list_me_in, true)
        : on_exit_old_range(entity, (EntityCtx *)other, list_me_out, true);
}

void OrthListAOI::on_shift_entity(EntityCtx *ctx, Ctx *other,
                                  EntityVector *list_other_in,
                                  EntityVector *list_other_out,
                                  int32_t shift_type)
{
    int32_t type = other->type();
    if (CT_ENTITY == type) return;

    EntityCtx *entity = other->entity();
    // 视野边界移动时是按顺序先右中左，或者左中右移动的，不应该会跨过自己本身的实体
    assert(entity != ctx);
    if (had_mark(entity)) return;

    // 实体向右移动遇到右边界或者向左移动遇到左边界，则是离开目标视野。其他则是进入目标视野
    entity->mark_ = now_mark_;
    shift_type == type
        // ctx离开entity的视野，需要使用ctx的旧坐标，entity的视野，特殊处理
        ? on_exit_old_pos_range(entity, ctx, list_other_out, false)
        : on_enter_range(entity, ctx, list_other_in, false);
}

int32_t OrthListAOI::update_visual(EntityId id, int32_t visual,
                                   EntityVector *list_me_in,
                                   EntityVector *list_me_out)
{
    EntityCtx *ctx = get_entity_ctx(id);
    if (!ctx)
    {
        ELOG("%s no ctx found: " FMT64d, __FUNCTION__, id);
        return -1;
    }

    bool had = ctx->has_visual();
    ctx->update_visual(visual);
    bool has = ctx->has_visual();

    // 视野从有变无
    if (had && !has)
    {
        ctx->update_old_pos();
        return remove_visual(ctx, list_me_out);
    }
    // 视野从无变有
    if (!had && has)
    {
        return insert_visual(ctx, list_me_in);
    }
    // 视野范围大小更新
    update_mark();
    // 视野变化需要对比旧位置、旧视野，保证旧位置和新位置同步
    ctx->update_old_pos();

    int32_t shift_type = ctx->visual_ - ctx->old_visual_;
    shift_visual<&Ctx::pos_x_, &Ctx::next_x_, &Ctx::prev_x_>(
        first_x_, ctx, list_me_in, list_me_out, shift_type);
    if (use_y_)
    {
        shift_visual<&Ctx::pos_y_, &Ctx::next_y_, &Ctx::prev_y_>(
            first_y_, ctx, list_me_in, list_me_out, shift_type);
    }
    shift_visual<&Ctx::pos_z_, &Ctx::next_z_, &Ctx::prev_z_>(
        first_z_, ctx, list_me_in, list_me_out, shift_type);
    return 0;
}

int32_t OrthListAOI::insert_visual(EntityCtx *ctx, EntityVector *list_in)
{
    insert_visual_list<&Ctx::pos_x_, &Ctx::next_x_, &Ctx::prev_x_>(
        first_x_, ctx,
        [this, ctx, list_in](Ctx *other)
        {
            if (CT_ENTITY == other->type())
            {
                on_enter_range(ctx, (EntityCtx *)other, list_in, true);
            }
        });
    if (use_y_)
    {
        insert_visual_list<&Ctx::pos_y_, &Ctx::next_y_, &Ctx::prev_y_>(
            first_y_, ctx, nullptr);
    }
    insert_visual_list<&Ctx::pos_z_, &Ctx::next_z_, &Ctx::prev_z_>(first_z_,
                                                                   ctx, nullptr);
    return 0;
}

int32_t OrthListAOI::remove_visual(EntityCtx *ctx, EntityVector *list_out)
{
    // 遍历旧视野区间，从其他实体interest列表删除自己，其他实体从自己视野消失
    each_range_entity(ctx, ctx->old_visual_,
                      [this, ctx, list_out](EntityCtx *other)
                      { on_exit_old_range(ctx, other, list_out, true); });

    Ctx *next_v = &(ctx->next_v_);
    Ctx *prev_v = &(ctx->prev_v_);
    // 从链表删除左右视野节点
    remove_list<&Ctx::next_x_, &Ctx::prev_x_>(first_x_, next_v);
    remove_list<&Ctx::next_x_, &Ctx::prev_x_>(first_x_, prev_v);
    if (use_y_)
    {
        remove_list<&Ctx::next_y_, &Ctx::prev_y_>(first_y_, next_v);
        remove_list<&Ctx::next_y_, &Ctx::prev_y_>(first_y_, prev_v);
    }
    remove_list<&Ctx::next_z_, &Ctx::prev_z_>(first_z_, next_v);
    remove_list<&Ctx::next_z_, &Ctx::prev_z_>(first_z_, prev_v);

    next_v->reset();
    prev_v->reset();
    return 0;
}

// /////////////////////////////////////////////////////////////////////////////
// 函数模板的实现放下面
// /////////////////////////////////////////////////////////////////////////////

template <int32_t OrthListAOI::Ctx::*pos_, OrthListAOI::Ctx *OrthListAOI::Ctx::*next_,
          OrthListAOI::Ctx *OrthListAOI::Ctx::*prev_>
void OrthListAOI::insert_list(Ctx *&list, Ctx *ctx,
                              std::function<void(Ctx *)> &&func)
{
    if (!list)
    {
        list = ctx;
        return;
    }

    Ctx *next = list;
    Ctx *prev = list->*prev_;
    while (next && ctx->comp<pos_>(next) > 0)
    {
        if (func) func(next);

        prev = next;
        next = next->*next_;
    }
    // 把ctx插入到prev与next之间
    prev ? prev->*next_ = ctx : list = ctx;
    ctx->*prev_ = prev;
    ctx->*next_ = next;
    if (next) next->*prev_ = ctx;
}

template <int32_t OrthListAOI::Ctx::*pos_, OrthListAOI::Ctx *OrthListAOI::Ctx::*next_,
          OrthListAOI::Ctx *OrthListAOI::Ctx::*prev_>
void OrthListAOI::insert_entity(Ctx *&list, EntityCtx *ctx,
                                std::function<void(Ctx *)> &&func)
{
    // 如果该实体不需要视野，则只插入单个实体，不插入视野边界
    // 依次插入视野左边界、实体、视野右边界，每一个都以上一个为起点进行遍历，以提高插入效率
    if (ctx->has_visual())
    {
        insert_list<pos_, next_, prev_>(
            list, &(ctx->prev_v_),
            std::forward<std::function<void(Ctx *)> &&>(func));

        Ctx *first = &(ctx->prev_v_);
        insert_list<pos_, next_, prev_>(
            first, ctx, std::forward<std::function<void(Ctx *)> &&>(func));

        first = ctx;
        insert_list<pos_, next_, prev_>(
            first, &(ctx->next_v_),
            std::forward<std::function<void(Ctx *)> &&>(func));
    }
    else
    {
        insert_list<pos_, next_, prev_>(
            list, ctx, std::forward<std::function<void(Ctx *)> &&>(func));
    }
}

template <OrthListAOI::Ctx *OrthListAOI::Ctx::*next_,
          OrthListAOI::Ctx *OrthListAOI::Ctx::*prev_>
void OrthListAOI::remove_list(Ctx *&list, Ctx *ctx)
{
    Ctx *prev = ctx->*prev_;
    Ctx *next = ctx->*next_;
    prev ? (prev->*next_ = next) : (list = next);
    if (next) next->*prev_ = prev;
}

template <OrthListAOI::Ctx *OrthListAOI::Ctx::*next_,
          OrthListAOI::Ctx *OrthListAOI::Ctx::*prev_>
void OrthListAOI::remove_entity(Ctx *&list, EntityCtx *ctx)
{
    bool has = ctx->has_visual();

    // 从三轴中删除自己(依次从右视野、实体、左视野删起)
    if (has)
    {
        remove_list<next_, prev_>(list, &(ctx->next_v_));
    }
    remove_list<next_, prev_>(list, ctx);
    if (has)
    {
        remove_list<next_, prev_>(list, &(ctx->prev_v_));
    }
}

template <int32_t OrthListAOI::Ctx::*new_, int32_t OrthListAOI::Ctx::*old_,
          OrthListAOI::Ctx *OrthListAOI::Ctx::*next_,
          OrthListAOI::Ctx *OrthListAOI::Ctx::*prev_>
void OrthListAOI::shift_entity(Ctx *&list, EntityCtx *ctx,
                               EntityVector *list_me_in,
                               EntityVector *list_other_in,
                               EntityVector *list_me_out,
                               EntityVector *list_other_out)
{
    now_mark_++;
    bool has = ctx->has_visual();
    if (ctx->*new_ > ctx->*old_)
    {
        if (has)
        {
            shift_list_next<new_, next_, prev_>(list, &(ctx->next_v_),
                                                list_me_in, list_other_in,
                                                list_me_out, list_other_out);
        }
        shift_list_next<new_, next_, prev_>(list, ctx, list_me_in, list_other_in,
                                            list_me_out, list_other_out);
        if (has)
        {
            shift_list_next<new_, next_, prev_>(list, &(ctx->prev_v_),
                                                list_me_in, list_other_in,
                                                list_me_out, list_other_out);
        }
    }
    else if (ctx->*new_ < ctx->*old_)
    {
        if (has)
        {
            shift_list_prev<new_, next_, prev_>(list, &(ctx->prev_v_),
                                                list_me_in, list_other_in,
                                                list_me_out, list_other_out);
        }
        shift_list_prev<new_, next_, prev_>(list, ctx, list_me_in, list_other_in,
                                            list_me_out, list_other_out);
        if (has)
        {
            shift_list_prev<new_, next_, prev_>(list, &(ctx->next_v_),
                                                list_me_in, list_other_in,
                                                list_me_out, list_other_out);
        }
    }
}

template <int32_t OrthListAOI::Ctx::*pos_, OrthListAOI::Ctx *OrthListAOI::Ctx::*next_,
          OrthListAOI::Ctx *OrthListAOI::Ctx::*prev_>
void OrthListAOI::shift_list_next(Ctx *&list, Ctx *ctx, EntityVector *list_me_in,
                                  EntityVector *list_other_in,
                                  EntityVector *list_me_out,
                                  EntityVector *list_other_out)
{
    bool is_entity = CT_ENTITY == ctx->type();

    Ctx *prev = nullptr;
    Ctx *next = ctx->*next_;
    while (next && ctx->comp<pos_>(next) > 0)
    {
        is_entity
            ? on_shift_entity((EntityCtx *)ctx, next, list_other_in,
                              list_other_out, CT_VISUAL_NEXT)
            : on_shift_visual(ctx, next, list_me_in, list_me_out, CT_VISUAL_NEXT);

        prev = next;
        next = next->*next_;
    }

    if (!prev) return; // 没有在链表中移动

    // 从原链表位置移除
    assert(prev != ctx && next != ctx);
    remove_list<next_, prev_>(list, ctx);

    // 把ctx插入到prev与next之间
    prev->*next_ = ctx;
    ctx->*prev_  = prev;
    ctx->*next_  = next;
    if (next) next->*prev_ = ctx;
}

template <int32_t OrthListAOI::Ctx::*pos_, OrthListAOI::Ctx *OrthListAOI::Ctx::*next_,
          OrthListAOI::Ctx *OrthListAOI::Ctx::*prev_>
void OrthListAOI::shift_list_prev(Ctx *&list, Ctx *ctx, EntityVector *list_me_in,
                                  EntityVector *list_other_in,
                                  EntityVector *list_me_out,
                                  EntityVector *list_other_out)
{
    bool is_entity = CT_ENTITY == ctx->type();

    Ctx *next = nullptr;
    Ctx *prev = ctx->*prev_;
    while (prev && ctx->comp<pos_>(prev) < 0)
    {
        is_entity
            ? on_shift_entity((EntityCtx *)ctx, prev, list_other_in,
                              list_other_out, CT_VISUAL_PREV)
            : on_shift_visual(ctx, prev, list_me_in, list_me_out, CT_VISUAL_PREV);
        next = prev;
        prev = prev->*prev_;
    }

    if (!next) return; // 没有在链表中移动

    // 从原链表位置移除
    assert(prev != ctx && next != ctx);
    remove_list<next_, prev_>(list, ctx);

    // 把ctx插入到prev与next之间
    prev ? (prev->*next_ = ctx) : (list = ctx);
    ctx->*prev_  = prev;
    ctx->*next_  = next;
    next->*prev_ = ctx;
}

template <int32_t OrthListAOI::Ctx::*pos_, OrthListAOI::Ctx *OrthListAOI::Ctx::*next_,
          OrthListAOI::Ctx *OrthListAOI::Ctx::*prev_>
void OrthListAOI::shift_visual(Ctx *&list, EntityCtx *ctx,
                               EntityVector *list_me_in,
                               EntityVector *list_me_out, int32_t shift_type)
{
    now_mark_++;
    if (shift_type > 0)
    {
        // 视野扩大，左边界左移，右边界右移
        shift_list_next<pos_, next_, prev_>(list, &(ctx->next_v_), list_me_in,
                                            nullptr, nullptr, nullptr);
        shift_list_prev<pos_, next_, prev_>(list, &(ctx->prev_v_), list_me_in,
                                            nullptr, nullptr, nullptr);
    }
    else if (shift_type < 0)
    {
        // 视野缩小，左边界右移，右边界左移
        shift_list_prev<pos_, next_, prev_>(list, &(ctx->next_v_), nullptr,
                                            nullptr, list_me_out, nullptr);
        shift_list_next<pos_, next_, prev_>(list, &(ctx->prev_v_), nullptr,
                                            nullptr, list_me_out, nullptr);
    }
}

template <int32_t OrthListAOI::Ctx::*pos_, OrthListAOI::Ctx *OrthListAOI::Ctx::*next_,
          OrthListAOI::Ctx *OrthListAOI::Ctx::*prev_>
void OrthListAOI::insert_visual_list(Ctx *&list, EntityCtx *ctx,
                                     std::function<void(Ctx *)> &&func)
{

    Ctx *prev = ctx;
    Ctx *next = ctx->*next_;

    // 向右遍历，把视野右边界插入到ctx右边合适的地方
    Ctx *next_v = &(ctx->next_v_);
    assert(!(next_v->*prev_) && !(next_v->*next_)); // 校验之前必定不在链表上
    while (next && next_v->comp<pos_>(next) > 0)
    {
        if (func) func(next);

        prev = next;
        next = next->*next_;
    }

    prev->*next_   = next_v;
    next_v->*prev_ = prev;
    next_v->*next_ = next;
    if (next) next->*prev_ = next_v;

    // 向左遍历，把视野左边界插入到ctx左边合适的地方
    next        = ctx;
    prev        = ctx->*prev_;
    Ctx *prev_v = &(ctx->prev_v_);
    assert(!(prev_v->*next_) && !(prev_v->*prev_)); // 校验之前必定不在链表上
    while (prev && prev_v->comp<pos_>(prev) < 0)
    {
        if (func) func(prev);

        next = prev;
        prev = prev->*prev_;
    }

    prev ? (prev->*next_ = prev_v) : (list = prev_v);
    prev_v->*prev_ = prev;
    prev_v->*next_ = next;
    next->*prev_   = prev_v;
}

void OrthListAOI::each_entity(std::function<bool(EntityCtx *)> &&func)
{
    // TODO 需要遍历特定坐标内的实体时，从三轴中的任意一轴都是等效，因此这里随意选择x轴
    Ctx *ctx = first_x_;
    while (ctx)
    {
        if (CT_ENTITY == ctx->type())
        {
            if (!func((EntityCtx *)ctx)) return;
        }
        ctx = ctx->next_x_;
    }
}

bool OrthListAOI::valid_dump(bool dump) const
{
#define DUMP_PRINTF(...) \
    if (dump) PLOG(__VA_ARGS__)

    bool ok   = true;
    Ctx *last = nullptr;
    DUMP_PRINTF("================ >>");
    Ctx *ctx = first_x_;
    DUMP_PRINTF("XXXXXXXXXXXXXXXX");
    while (ctx)
    {
        EntityId id = CT_ENTITY == ctx->type() ? ((EntityCtx *)ctx)->id_
                                               : ctx->entity()->id_;
        DUMP_PRINTF("(type = %2d, id = " FMT64d ", x = %5d, y = %5d, z = %5d",
                    ctx->type(), id, ctx->pos_x_, ctx->pos_y_, ctx->pos_z_);
        if (last && last->comp<&Ctx::pos_x_>(ctx) > 0)
        {
            DUMP_PRINTF("dump error");
            ok = false;
        }
        last = ctx;
        ctx  = ctx->next_x_;
    }

    last = nullptr;
    ctx  = first_y_;
    DUMP_PRINTF("YYYYYYYYYYYYYYYY");
    while (ctx)
    {
        EntityId id = CT_ENTITY == ctx->type() ? ((EntityCtx *)ctx)->id_
                                               : ctx->entity()->id_;
        DUMP_PRINTF("(type = %2d, id = " FMT64d ", x = %5d, y = %5d, z = %5d",
                    ctx->type(), id, ctx->pos_x_, ctx->pos_y_, ctx->pos_z_);
        if (last && last->comp<&Ctx::pos_y_>(ctx) > 0)
        {
            DUMP_PRINTF("dump error");
            ok = false;
        }
        last = ctx;
        ctx  = ctx->next_y_;
    }

    last = nullptr;
    ctx  = first_z_;
    DUMP_PRINTF("ZZZZZZZZZZZZZZZZ");
    while (ctx)
    {
        EntityId id = CT_ENTITY == ctx->type() ? ((EntityCtx *)ctx)->id_
                                               : ctx->entity()->id_;
        DUMP_PRINTF("(type = %2d, id = " FMT64d ", x = %5d, y = %5d, z = %5d",
                    ctx->type(), id, ctx->pos_x_, ctx->pos_y_, ctx->pos_z_);
        if (last && last->comp<&Ctx::pos_z_>(ctx) > 0)
        {
            DUMP_PRINTF("dump error");
            ok = false;
        }
        last = ctx;
        ctx  = ctx->next_z_;
    }
    DUMP_PRINTF("================ <<");

#undef DUMP_PRINTF

    return ok;
}
