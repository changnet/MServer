#include "list_aoi.h"

////////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////

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

void ListAOI::each_range_entity(Ctx *ctx, int32_t visual,
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

    Ctx *cast_ctx    = ctx;
    Ctx *cast_prev_v = &(ctx->_prev_v);

    // 先插入x轴视野左边界
    insert_list<&Ctx::_pos_x, &Ctx::_next_x, &Ctx::_prev_x>(
        _first_x, &(ctx->_prev_v), nullptr);

    // 插入x轴实体，并建立建立interest me列表
    insert_list<&Ctx::_pos_x, &Ctx::_next_x, &Ctx::_prev_x>(
        cast_prev_v, ctx, [this, ctx, list_me_in, list_other_in](Ctx *other) {
            int32_t type = other->type();
            // 进入对方视野范围
            if (CT_VISUAL_PREV == type)
            {
                on_enter_range(ctx, other->entity(), list_other_in);
            }
            // 对方进入我的视野范围
            if (CT_ENTITY == type)
            {
                on_enter_range(other->entity(), ctx, list_me_in);
            }
        });

    // 插入x轴视野右边界
    insert_list<&Ctx::_pos_x, &Ctx::_next_x, &Ctx::_prev_x>(
        cast_ctx, &(ctx->_next_v),
        [this, ctx, list_me_in, list_other_in](Ctx *other) {
            int32_t type = other->type();
            // 进入对方视野范围
            if (CT_VISUAL_NEXT == type)
            {
                on_enter_range(ctx, other->entity(), list_other_in);
            }
            // 对方进入我的视野范围
            if (CT_ENTITY == type)
            {
                on_enter_range(other->entity(), ctx, list_me_in);
            }
        });

    // 插入另外的二轴，由于x轴已经遍历了，所以这里不需要再处理视野问题
    if (_use_y)
    {
        insert_list<&Ctx::_pos_y, &Ctx::_next_y, &Ctx::_prev_y>(
            _first_y, &(ctx->_prev_v), nullptr);
        insert_list<&Ctx::_pos_y, &Ctx::_next_y, &Ctx::_prev_y>(cast_prev_v,
                                                                ctx, nullptr);
        insert_list<&Ctx::_pos_y, &Ctx::_next_y, &Ctx::_prev_y>(
            cast_ctx, &(ctx->_next_v), nullptr);
    }
    insert_list<&Ctx::_pos_z, &Ctx::_next_z, &Ctx::_prev_z>(_first_z, ctx,
                                                            nullptr);
    insert_list<&Ctx::_pos_z, &Ctx::_next_z, &Ctx::_prev_z>(cast_prev_v, ctx,
                                                            nullptr);
    insert_list<&Ctx::_pos_z, &Ctx::_next_z, &Ctx::_prev_z>(
        cast_ctx, &(ctx->_next_v), nullptr);

    return true;
}

int32_t ListAOI::exit_entity(EntityId id, EntityVector *list)
{
    EntitySet::iterator iter = _entity_set.find(id);
    if (_entity_set.end() == iter) return 1;

    EntityCtx *ctx = iter->second;
    _entity_set.erase(iter);

    // 遍历整个视野范围，从别人的interest_me列表删除自己
    if (ctx->_mask & INTEREST)
    {
        Ctx *next = ctx->_prev_v._next_x;
        while (next != ctx)
        {
            assert(next);
            if (CT_VISUAL_PREV == next->type())
            {
                EntityCtx *entity = next->entity();
                if (in_visual(entity, ctx->_pos_x, ctx->_pos_y, ctx->_pos_z))
                {
                    remove_entity_from_vector(entity->_interest_me, ctx);
                }
            }
        }

        next = ctx->_next_x;
        while (next != &(ctx->_next_v))
        {
            assert(next);
            if (CT_VISUAL_NEXT == next->type())
            {
                EntityCtx *entity = next->entity();
                if (in_visual(entity, ctx->_pos_x, ctx->_pos_y, ctx->_pos_z))
                {
                    remove_entity_from_vector(entity->_interest_me, ctx);
                }
            }
        }
    }

    // 从三轴中删除自己(依次从右视野、实体、左视野删起)
    remove_list<&Ctx::_next_x, &Ctx::_prev_x>(_first_x, &(ctx->_next_v));
    remove_list<&Ctx::_next_x, &Ctx::_prev_x>(_first_x, ctx);
    remove_list<&Ctx::_next_x, &Ctx::_prev_x>(_first_x, &(ctx->_prev_v));
    if (_use_y)
    {
        remove_list<&Ctx::_next_y, &Ctx::_prev_y>(_first_y, &(ctx->_next_v));
        remove_list<&Ctx::_next_y, &Ctx::_prev_y>(_first_y, ctx);
        remove_list<&Ctx::_next_y, &Ctx::_prev_y>(_first_y, &(ctx->_prev_v));
    }
    remove_list<&Ctx::_next_z, &Ctx::_prev_z>(_first_z, &(ctx->_next_v));
    remove_list<&Ctx::_next_z, &Ctx::_prev_z>(_first_z, ctx);
    remove_list<&Ctx::_next_z, &Ctx::_prev_z>(_first_z, &(ctx->_prev_v));

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

    int32_t shift_type = visual - ctx->_visual;
    ctx->update_visual(visual);
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
