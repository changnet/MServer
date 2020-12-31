#include "list_aoi.h"

// /////////////////////////////////////////////////////////////////////////////

void ListAOI::Ctx::reset()
{
    _pos_x = 0;
    _pos_y = 0;
    _pos_z = 0;
    _old_x = 0;
    _old_y = 0;
    _old_z = 0;

    _next_x = nullptr;
    _prev_x = nullptr;
    _next_y = nullptr;
    _prev_y = nullptr;
    _next_z = nullptr;
    _prev_z = nullptr;
}

void ListAOI::Ctx::update_pos(int32_t x, int32_t y, int32_t z)
{
    _old_x = _pos_x;
    _old_y = _pos_y;
    _old_z = _pos_z;
    _pos_x = x;
    _pos_y = y;
    _pos_z = z;
}

void ListAOI::EntityCtx::reset()
{
    Ctx::reset();

    _mask        = 0;
    _id          = 0;
    _tick        = 0;
    _visual      = 0;
    _old_visual  = 0;
    _interest_me = nullptr;

    _next_v.reset();
    _prev_v.reset();
}

void ListAOI::EntityCtx::update_visual(int32_t visual)
{
    if (visual >= 0)
    {
        _old_visual = _visual;
        _visual     = visual;
    }

    _prev_v.update_pos(_pos_x - _visual, _pos_y - _visual, _pos_z - _visual);
    _next_v.update_pos(_pos_x + _visual, _pos_y + _visual, _pos_z + _visual);
}

// /////////////////////////////////////////////////////////////////////////////

ListAOI::ListAOI()
{
    _first_x = nullptr;
    _first_y = nullptr;
    _first_z = nullptr;

    _use_y = true;
    _tick  = 1; // 不能为0，不可以与EntityCtx中_tick的默认值一致
}

ListAOI::~ListAOI()
{
    for (auto &iter : _entity_set) del_entity_ctx(iter.second);
}

// 获取实体的ctx
struct ListAOI::EntityCtx *ListAOI::get_entity_ctx(EntityId id)
{
    EntitySet::const_iterator itr = _entity_set.find(id);
    if (_entity_set.end() == itr) return nullptr;

    return itr->second;
}

bool ListAOI::remove_entity_from_vector(EntityVector *list, const EntityCtx *ctx)
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

void ListAOI::each_range_entity(const Ctx *ctx, int32_t visual,
                                std::function<void(EntityCtx *)> &&func)
{
    // 实体同时存在三轴链表上，只需要遍历其中一个链表即可

    // 往链表左边遍历
    int32_t prev_visual = ctx->_pos_x - visual;
    Ctx *prev           = ctx->_prev_x;
    while (prev && prev->_pos_x >= prev_visual)
    {
        if (CT_ENTITY == prev->type()) func((EntityCtx *)prev);
        prev = prev->_prev_x;
    }

    // 往链表右边遍历
    int32_t next_visual = ctx->_pos_x + visual;
    Ctx *next           = ctx->_next_x;
    while (next && next->_pos_x <= next_visual)
    {
        if (CT_ENTITY == prev->type()) func((EntityCtx *)next);
        next = next->_next_x;
    }
}

void ListAOI::on_enter_range(EntityCtx *ctx, EntityCtx *other,
                             EntityVector *list_in)
{
    // 这里只是表示进入一个轴，最终是否在视野范围内要判断三轴
    if (in_visual(ctx, other->_pos_x, other->_pos_y, other->_pos_z))
    {
        if (ctx->_mask & INTEREST)
        {
            other->_interest_me->push_back(ctx);
        }
        if (list_in) list_in->emplace_back(other);
    }
}

void ListAOI::on_exit_range(EntityCtx *ctx, EntityCtx *other,
                            EntityVector *list_out)
{
    // 判断旧视野
    if (in_visual(ctx, other->_pos_x, other->_pos_y, other->_pos_z))
    {
        if (ctx->_mask & INTEREST)
        {
            remove_entity_from_vector(other->_interest_me, ctx);
        }
        if (list_out) list_out->emplace_back(other);
    }
}

void ListAOI::on_exit_old_range(EntityCtx *ctx, EntityCtx *other,
                                EntityVector *list_out)
{
    // 判断旧视野
    if (in_visual(other, ctx->_old_x, ctx->_old_y, ctx->_old_z, ctx->_old_visual))
    {
        if (ctx->_mask & INTEREST)
        {
            remove_entity_from_vector(other->_interest_me, ctx);
        }
        if (list_out) list_out->emplace_back(other);
    }
}

bool ListAOI::enter_entity(EntityId id, int32_t x, int32_t y, int32_t z,
                           int32_t visual, uint8_t mask,
                           EntityVector *list_me_in, EntityVector *list_other_in)
{
    // 防止重复进入场景
    auto ret = _entity_set.emplace(id, nullptr);
    if (false == ret.second)
    {
        ERROR("%s entity already in scene " FMT64d, __FUNCTION__, id);
        return false;
    }

    EntityCtx *ctx    = new_entity_ctx();
    ret.first->second = ctx;

    ctx->_id   = id;
    ctx->_mask = mask;
    ctx->update_pos(x, y, z);
    ctx->update_visual(visual);

    insert_entity<&Ctx::_pos_x, &Ctx::_next_x, &Ctx::_prev_x>(
        _first_x, ctx, [this, ctx, list_me_in, list_other_in](Ctx *other) {
            CT_ENTITY == other->type()
                // 对方进入我的视野范围
                ? on_enter_range(ctx, other->entity(), list_me_in)
                // 我进入对方视野范围
                : on_enter_range(other->entity(), ctx, list_other_in);
        });
    // 插入另外的二轴，由于x轴已经遍历了，所以这里不需要再处理视野问题
    if (_use_y)
    {
        insert_entity<&Ctx::_pos_y, &Ctx::_next_y, &Ctx::_prev_y>(_first_y, ctx,
                                                                  nullptr);
    }
    insert_entity<&Ctx::_pos_z, &Ctx::_next_z, &Ctx::_prev_z>(_first_z, ctx,
                                                              nullptr);

    return true;
}

int32_t ListAOI::exit_entity(EntityId id, EntityVector *list)
{
    EntitySet::iterator iter = _entity_set.find(id);
    if (_entity_set.end() == iter) return 1;

    EntityCtx *ctx = iter->second;
    _entity_set.erase(iter);

    // 从别人的interest_me列表删除自己
    if (ctx->has_visual())
    {
        each_range_entity(ctx, ctx->_visual, [this, ctx](EntityCtx *other) {
            on_exit_range(ctx, other, nullptr);
        });
    }

    // 从三轴中删除自己
    remove_entity<&Ctx::_next_x, &Ctx::_prev_x>(_first_x, ctx);
    if (_use_y)
    {
        remove_entity<&Ctx::_next_y, &Ctx::_prev_y>(_first_y, ctx);
    }
    remove_entity<&Ctx::_next_z, &Ctx::_prev_z>(_first_z, ctx);

    // 是否需要返回关注自己离开场景的实体列表
    if (list)
    {
        EntityVector *interest_me = ctx->_interest_me;
        list->insert(list->end(), interest_me->begin(), interest_me->end());
    }

    del_entity_ctx(ctx);

    return 0;
}

int32_t ListAOI::update_entity(EntityId id, int32_t x, int32_t y, int32_t z,
                               EntityVector *list_me_in,
                               EntityVector *list_other_in,
                               EntityVector *list_me_out,
                               EntityVector *list_other_out)
{
    struct EntityCtx *ctx = get_entity_ctx(id);
    if (!ctx)
    {
        ERROR("%s no ctx found: " FMT64d, __FUNCTION__, id);
        return -1;
    }

    /**
     * 在三轴中移动时，遍历的实体可能会出现重复，有几种方式去重
     * 1. 用std::map或者std::unordered_map去重
     * 2. 遍历数组去重
     * 3. 通过判断坐标，是否重复在其他轴出现过
     * 4. 在每个实体上加一个计数器，每次遍历到就加1，通过计数器判断是否重复(计数器重置则需要
     *    重置所有实体的计数器，避免重复
     */
    _tick++;
    if (_tick == 0xFFFFFFFF)
    {
        _tick = 1;
        for (auto iter = _entity_set.begin(); iter != _entity_set.end(); iter++)
        {
            iter->second->_tick = 0;
        }
    }

    ctx->update_pos(x, y, z); // 先设置新坐标
    ctx->update_visual(-1);   // 更新视野坐标

    shift_entity<&Ctx::_pos_x, &Ctx::_old_x, &Ctx::_next_x, &Ctx::_prev_x>(
        _first_x, ctx, list_me_in, list_other_in, list_me_out, list_other_out);
    if (_use_y)
    {
        shift_entity<&Ctx::_pos_y, &Ctx::_old_y, &Ctx::_next_y, &Ctx::_prev_y>(
            _first_y, ctx, list_me_in, list_other_in, list_me_out,
            list_other_out);
    }
    shift_entity<&Ctx::_pos_z, &Ctx::_old_z, &Ctx::_next_z, &Ctx::_prev_z>(
        _first_z, ctx, list_me_in, list_other_in, list_me_out, list_other_out);

    return 0;
}

void ListAOI::on_shift_visual(EntityCtx *ctx, Ctx *other,
                              EntityVector *list_me_in,
                              EntityVector *list_me_out, int32_t shift_type)
{
    int32_t type = other->type();
    if (CT_ENTITY != type || ((EntityCtx *)other)->_tick == _tick) return;

    ((EntityCtx *)other)->_tick = _tick;

    // 向各移动右边界或者向左移动左边界，检测其他实体进入视野。其他情况检测退出视野
    shift_type == type ? on_enter_range(((EntityCtx *)other), ctx, list_me_in)
                       : on_exit_old_range(ctx, (EntityCtx *)other, list_me_out);
}

void ListAOI::on_shift_entity(EntityCtx *ctx, Ctx *other,
                              EntityVector *list_other_in,
                              EntityVector *list_other_out, int32_t shift_type)
{
    int32_t type = other->type();
    if (CT_ENTITY == type) return;

    EntityCtx *entity = other->entity();
    if (entity->_tick == _tick) return;

    // 实体向右移动遇到右边界或者向左移动遇到左边界，则是离开目标视野。其他则是进入目标视野
    entity->_tick = _tick;
    shift_type == type ? on_exit_old_range(entity, ctx, list_other_out)
                       : on_enter_range(ctx, entity, list_other_in);
}

int32_t ListAOI::update_visual(EntityId id, int32_t visual,
                               EntityVector *list_me_in,
                               EntityVector *list_me_out)
{
    struct EntityCtx *ctx = get_entity_ctx(id);
    if (!ctx)
    {
        ERROR("%s no ctx found: " FMT64d, __FUNCTION__, id);
        return -1;
    }

    bool had = ctx->has_visual();
    ctx->update_visual(visual);
    bool has = ctx->has_visual();

    // 视野从有变无
    if (had && !has)
    {
        return remove_visual(ctx, list_me_out);
    }
    // 视野从无变有
    if (!had && has)
    {
        return insert_visual(ctx, list_me_in);
    }
    // 视野范围大小更新
    int32_t shift_type = ctx->_visual - ctx->_old_visual;
    shift_visual<&Ctx::_pos_x, &Ctx::_next_x, &Ctx::_prev_x>(
        _first_x, ctx, list_me_in, list_me_out, shift_type);
    if (_use_y)
    {
        shift_visual<&Ctx::_pos_y, &Ctx::_next_y, &Ctx::_prev_y>(
            _first_y, ctx, list_me_in, list_me_out, shift_type);
    }
    shift_visual<&Ctx::_pos_z, &Ctx::_next_z, &Ctx::_prev_z>(
        _first_z, ctx, list_me_in, list_me_out, shift_type);
    return 0;
}

int32_t ListAOI::insert_visual(EntityCtx *ctx, EntityVector *list_in)
{
    insert_visual_list<&Ctx::_pos_x, &Ctx::_next_x, &Ctx::_prev_x>(
        _first_x, ctx, [this, ctx, list_in](Ctx *other) {
            if (CT_ENTITY == other->type())
            {
                on_enter_range(ctx, (EntityCtx *)other, list_in);
            }
        });
    if (_use_y)
    {
        insert_visual_list<&Ctx::_pos_y, &Ctx::_next_y, &Ctx::_prev_y>(
            _first_y, ctx, nullptr);
    }
    insert_visual_list<&Ctx::_pos_z, &Ctx::_next_z, &Ctx::_prev_z>(_first_z,
                                                                   ctx, nullptr);
    return 0;
}

int32_t ListAOI::remove_visual(EntityCtx *ctx, EntityVector *list_out)
{
    // 遍历旧视野区间，从其他实体interest列表删除自己，其他实体从自己视野消失
    each_range_entity(ctx, ctx->_old_visual,
                      [this, ctx, list_out](EntityCtx *other) {
                          on_exit_old_range(ctx, other, list_out);
                      });

    Ctx *next_v = &(ctx->_next_v);
    Ctx *prev_v = &(ctx->_prev_v);
    // 从链表删除左右视野节点
    remove_list<&Ctx::_next_x, &Ctx::_prev_x>(_first_x, next_v);
    remove_list<&Ctx::_next_x, &Ctx::_prev_x>(_first_x, prev_v);
    if (_use_y)
    {
        remove_list<&Ctx::_next_y, &Ctx::_prev_y>(_first_y, next_v);
        remove_list<&Ctx::_next_y, &Ctx::_prev_y>(_first_y, prev_v);
    }
    remove_list<&Ctx::_next_z, &Ctx::_prev_z>(_first_z, next_v);
    remove_list<&Ctx::_next_z, &Ctx::_prev_z>(_first_z, prev_v);

    next_v->reset();
    prev_v->reset();
    return 0;
}

// /////////////////////////////////////////////////////////////////////////////
// 函数模板的实现放下面
// /////////////////////////////////////////////////////////////////////////////

template <int32_t ListAOI::Ctx::*_pos, ListAOI::Ctx *ListAOI::Ctx::*_next,
          ListAOI::Ctx *ListAOI::Ctx::*_prev>
void ListAOI::insert_list(Ctx *&list, Ctx *ctx, std::function<void(Ctx *)> &&func)
{
    if (!list)
    {
        list = ctx;
        return;
    }

    Ctx *next = list;
    Ctx *prev = nullptr;
    while (next && ctx->comp<_pos>(next) < 0)
    {
        if (func) func(next);

        prev = next;
        next = next->*_next;
    }
    // 把ctx插入到prev与next之间
    if (prev) prev->*_next = ctx;
    ctx->*_prev = prev;
    ctx->*_next = next;
    if (next) next->*_prev = ctx;
}

template <int32_t ListAOI::Ctx::*_pos, ListAOI::Ctx *ListAOI::Ctx::*_next,
          ListAOI::Ctx *ListAOI::Ctx::*_prev>
void ListAOI::insert_entity(Ctx *&list, EntityCtx *ctx,
                            std::function<void(Ctx *)> &&func)
{
    // 如果该实体不需要视野，则只插入单个实体，不插入视野边界，以
    // 依次插入视野左边界、实体、视野右边界，每一个都以上一个为起点进行遍历，以提高插入效率
    bool has = ctx->has_visual();

    Ctx *&first = _first_x;
    if (has)
    {
        insert_list<_pos, _next, _prev>(
            list, &(ctx->_prev_v),
            std::forward<std::function<void(Ctx *)> &&>(func));
        first = &(ctx->_prev_v);
    }
    insert_list<_pos, _next, _prev>(
        first, ctx, std::forward<std::function<void(Ctx *)> &&>(func));
    if (has)
    {
        first = ctx;
        insert_list<_pos, _next, _prev>(
            first, &(ctx->_next_v),
            std::forward<std::function<void(Ctx *)> &&>(func));
    }
}

template <ListAOI::Ctx *ListAOI::Ctx::*_next, ListAOI::Ctx *ListAOI::Ctx::*_prev>
void ListAOI::remove_list(Ctx *&list, Ctx *ctx)
{
    Ctx *prev = ctx->*_prev;
    Ctx *next = ctx->*_next;
    prev ? (prev->*_next = next) : (list = next);
    if (next) next->*_prev = prev;
}

template <ListAOI::Ctx *ListAOI::Ctx::*_next, ListAOI::Ctx *ListAOI::Ctx::*_prev>
void ListAOI::remove_entity(Ctx *&list, EntityCtx *ctx)
{
    bool has = ctx->has_visual();

    // 从三轴中删除自己(依次从右视野、实体、左视野删起)
    if (has)
    {
        remove_list<_next, _prev>(list, &(ctx->_next_v));
    }
    remove_list<_next, _prev>(list, ctx);
    if (has)
    {
        remove_list<_next, _prev>(list, &(ctx->_prev_v));
    }
}

template <int32_t ListAOI::Ctx::*_new, int32_t ListAOI::Ctx::*_old,
          ListAOI::Ctx *ListAOI::Ctx::*_next, ListAOI::Ctx *ListAOI::Ctx::*_prev>
void ListAOI::shift_entity(Ctx *&list, EntityCtx *ctx, EntityVector *list_me_in,
                           EntityVector *list_other_in, EntityVector *list_me_out,
                           EntityVector *list_other_out)
{
    bool has = ctx->has_visual();
    if (ctx->*_new > ctx->*_old)
    {
        if (has)
        {
            shift_list_next<_new, _next, _prev>(list, &(ctx->_next_v),
                                                list_me_in, list_other_in,
                                                list_me_out, list_other_out);
        }
        shift_list_next<_new, _next, _prev>(list, ctx, list_me_in, list_other_in,
                                            list_me_out, list_other_out);
        if (has)
        {
            shift_list_next<_new, _next, _prev>(list, &(ctx->_prev_v),
                                                list_me_in, list_other_in,
                                                list_me_out, list_other_out);
        }
    }
    else if (ctx->*_new < ctx->*_old)
    {
        if (has)
        {
            shift_list_prev<_new, _next, _prev>(list, &(ctx->_prev_v),
                                                list_me_in, list_other_in,
                                                list_me_out, list_other_out);
        }
        shift_list_prev<_new, _next, _prev>(list, ctx, list_me_in, list_other_in,
                                            list_me_out, list_other_out);
        if (has)
        {
            shift_list_prev<_new, _next, _prev>(list, &(ctx->_next_v),
                                                list_me_in, list_other_in,
                                                list_me_out, list_other_out);
        }
    }
}

template <int32_t ListAOI::Ctx::*_pos, ListAOI::Ctx *ListAOI::Ctx::*_next,
          ListAOI::Ctx *ListAOI::Ctx::*_prev>
void ListAOI::shift_list_next(Ctx *&list, Ctx *ctx, EntityVector *list_me_in,
                              EntityVector *list_other_in,
                              EntityVector *list_me_out,
                              EntityVector *list_other_out)
{
    bool is_entity = CT_ENTITY == ctx->type();

    Ctx *prev = nullptr;
    Ctx *next = ctx->*_next;
    while (next && ctx->comp<_pos>(next) < 0)
    {
        is_entity ? on_shift_entity((EntityCtx *)ctx, next, list_other_in,
                                    list_other_out, CT_VISUAL_NEXT)
                  : on_shift_visual(ctx->entity(), next, list_me_in,
                                    list_me_out, CT_VISUAL_NEXT);

        prev = next;
        next = next->*_next;
    }

    if (!prev) return; // 没有在链表中移动

    // 从原链表位置移除
    assert(prev != ctx && next != ctx);
    remove_list<_next, _prev>(list, ctx);

    // 把ctx插入到prev与next之间
    prev->*_next = ctx;
    ctx->*_prev  = prev;
    ctx->*_next  = next;
    if (next) next->*_prev = ctx;
}

template <int32_t ListAOI::Ctx::*_pos, ListAOI::Ctx *ListAOI::Ctx::*_next,
          ListAOI::Ctx *ListAOI::Ctx::*_prev>
void ListAOI::shift_list_prev(Ctx *&list, Ctx *ctx, EntityVector *list_me_in,
                              EntityVector *list_other_in,
                              EntityVector *list_me_out,
                              EntityVector *list_other_out)
{
    bool is_entity = CT_ENTITY == ctx->type();

    Ctx *next = nullptr;
    Ctx *prev = ctx->*_prev;
    while (prev && ctx->comp<_pos>(prev) > 0)
    {
        is_entity ? on_shift_entity((EntityCtx *)ctx, prev, list_other_in,
                                    list_other_out, CT_VISUAL_PREV)
                  : on_shift_visual(ctx->entity(), prev, list_me_in,
                                    list_me_out, CT_VISUAL_PREV);
        next = prev;
        prev = prev->*_prev;
    }

    if (!next) return; // 没有在链表中移动

    // 从原链表位置移除
    assert(prev != ctx && next != ctx);
    remove_list<_next, _prev>(list, ctx);

    // 把ctx插入到prev与next之间
    prev ? (prev->*_next = ctx) : (list = ctx);
    ctx->*_prev  = prev;
    ctx->*_next  = next;
    next->*_prev = ctx;
}

template <int32_t ListAOI::Ctx::*_pos, ListAOI::Ctx *ListAOI::Ctx::*_next,
          ListAOI::Ctx *ListAOI::Ctx::*_prev>
void ListAOI::shift_visual(Ctx *&list, EntityCtx *ctx, EntityVector *list_me_in,
                           EntityVector *list_me_out, int32_t shift_type)
{
    if (shift_type > 0)
    {
        // 视野扩大，左边界左移，右边界右移
        shift_list_next<_pos, _next, _prev>(list, &(ctx->_next_v), list_me_in,
                                            nullptr, list_me_out, nullptr);
        shift_list_prev<_pos, _next, _prev>(list, &(ctx->_prev_v), list_me_in,
                                            nullptr, list_me_out, nullptr);
    }
    else if (shift_type < 0)
    {
        // 视野缩小，左边界右移，右边界左移
        shift_list_prev<_pos, _next, _prev>(list, &(ctx->_prev_v), list_me_in,
                                            nullptr, list_me_out, nullptr);
        shift_list_next<_pos, _next, _prev>(list, &(ctx->_next_v), list_me_in,
                                            nullptr, list_me_out, nullptr);
    }
}

template <int32_t ListAOI::Ctx::*_pos, ListAOI::Ctx *ListAOI::Ctx::*_next,
          ListAOI::Ctx *ListAOI::Ctx::*_prev>
void ListAOI::insert_visual_list(Ctx *&list, EntityCtx *ctx,
                                 std::function<void(Ctx *)> &&func)
{

    Ctx *prev = ctx;
    Ctx *next = ctx->*_next;

    // 向右遍历，把视野右边界插入到ctx右边合适的地方
    Ctx *next_v = &(ctx->_next_v);
    assert(!(next_v->*_prev) && !(next_v->*_next)); // 校验之前必定不在链表上
    while (next && next_v->comp<_pos>(next) < 0)
    {
        if (func) func(next);

        prev = next;
        next = next->*_next;
    }

    prev->*_next   = next_v;
    next_v->*_prev = prev;
    next_v->*_next = next;
    if (next) next->*_prev = next_v;

    // 向左遍历，把视野左边界插入到ctx左边合适的地方
    next        = ctx;
    prev        = ctx->*_prev;
    Ctx *prev_v = &(ctx->_prev_v);
    assert(!(prev_v->*_next) && !(prev_v->*_prev)); // 校验之前必定不在链表上
    while (prev && prev_v->comp<_pos>(prev) > 0)
    {
        if (func) func(prev);

        next = prev;
        prev = prev->*_prev;
    }

    prev ? (prev->*_next = prev_v) : (list = prev_v);
    prev_v->*_prev = prev;
    prev_v->*_next = next;
    next->*_prev   = prev_v;
}

void ListAOI::each_entity(std::function<bool(EntityCtx *)> &&func)
{
    // TODO 需要遍历特定坐标内的实体时，从三轴中的任意一轴都是等效，因此这里随意选择x轴
    Ctx *ctx = _first_x;
    while (ctx)
    {
        if (CT_ENTITY == ctx->type())
        {
            if (!func((EntityCtx *)ctx)) return;
        }
        ctx = ctx->_next_x;
    }
}
