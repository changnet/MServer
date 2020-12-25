#include "list_aoi.h"

////////////////////////////////////////////////////////////////////////////////
void ListAOI::Ctx::reset()
{
    _pos_x = 0;
    _pos_y = 0;
    _pos_z = 0;

    _next_x = nullptr;
    _prev_x = nullptr;
    _next_y = nullptr;
    _prev_y = nullptr;
    _next_z = nullptr;
    _prev_z = nullptr;
}

void ListAOI::EntityCtx::reset()
{
    Ctx::reset();

    _mask        = 0;
    _id          = 0;
    _interest_me = nullptr;

    _next_v.reset();
    _prev_v.reset();
}

////////////////////////////////////////////////////////////////////////////////

ListAOI::ListAOI()
{
    _first_x = nullptr;
    _first_y = nullptr;
    _first_z = nullptr;

    _use_y      = true;
    _max_visual = 0;
}

ListAOI::~ListAOI()
{
    for (auto &iter : _entity_set) del_entity_ctx(iter.second);
}

bool ListAOI::remove_entity_from_vector(EntityVector *list,
                                        const struct EntityCtx *ctx)
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

    struct EntityCtx *ctx = new_entity_ctx();
    ret.first->second     = ctx;

    ctx->_id     = id;
    ctx->_mask   = mask;
    ctx->_pos_x  = x;
    ctx->_pos_y  = y;
    ctx->_pos_z  = z;
    ctx->_visual = visual;

    bool interest = mask & INTEREST;

    // 分别插入三轴链表

    // 先插入x轴左边界
    insert_list<&Ctx::_pos_x, &Ctx::_next_x, &Ctx::_prev_x>(
        _first_x, &(ctx->_prev_v), nullptr);
    // 插入x轴实体，并建立建立interest me列表
    Ctx *prev_v = &(ctx->_prev_v);
    insert_list<&Ctx::_pos_x, &Ctx::_next_x, &Ctx::_prev_x>(
        prev_v, ctx,
        [this, ctx, list_me_in, list_other_in, interest, x, y, z](Ctx *other) {
            int32_t type = other->type();
            // 进入对方视野范围
            if (CT_VISUAL_PREV == type)
            {
                EntityCtx *entity = other->entity();
                if (in_visual(entity, x, y, z))
                {
                    if (entity->_mask & INTEREST)
                    {
                        ctx->_interest_me->emplace_back(entity);
                    }
                    if (list_other_in) list_other_in->emplace_back(entity);
                }
            }

            // 对方进入我的视野范围
            if (CT_ENTITY == type
                && in_visual(ctx, other->_pos_x, other->_pos_y, other->_pos_z))
            {
                if (interest)
                {
                    ((EntityCtx *)other)->_interest_me->push_back(ctx);
                }
                if (list_me_in) list_me_in->emplace_back((EntityCtx *)other);
            }
        });
    if (_use_y)
    {
        insert_list<&Ctx::_pos_y, &Ctx::_next_y, &Ctx::_prev_y>(_first_x, ctx,
                                                                nullptr);
    }
    insert_list<&Ctx::_pos_z, &Ctx::_next_z, &Ctx::_prev_z>(_first_x, ctx,
                                                            nullptr);

    // 建立interest me列表
    each_range_entity(
        ctx, _max_visual,
        [this, ctx, list_me_in, interest, x, y, z](EntityCtx *other) {
            if ((other->_mask & INTEREST) && in_visual(other, x, y, z))
            {
                ctx->_interest_me->emplace_back(other);
            }
            if (interest
                && in_visual(ctx, other->_pos_x, other->_pos_y, other->_pos_z))
            {
                other->_interest_me->push_back(ctx);
            }
            if (list_me_in) list_me_in->emplace_back(other);
        });

    return true;
}

int32_t ListAOI::exit_entity(EntityId id, EntityVector *list)
{
    EntitySet::iterator iter = _entity_set.find(id);
    if (_entity_set.end() == iter) return 1;

    struct EntityCtx *ctx = iter->second;
    _entity_set.erase(iter);

    // 从别人的interest_me列表删除(自己关注event才有可能出现在别人的interest列表中)
    if (ctx->_mask & INTEREST)
    {
        each_range_entity(ctx, _max_visual, [this, ctx](EntityCtx *other) {
            if (in_visual(other, ctx->_pos_x, ctx->_pos_y, ctx->_pos_z))
            {
                remove_entity_from_vector(other->_interest_me, ctx);
            }
        });
    }

    // 从三轴中删除自己
    remove_list<&Ctx::_next_x, &Ctx::_prev_x>(_first_x, ctx);
    if (_use_y)
    {
        remove_list<&EntityCtx::_next_y, &EntityCtx::_prev_y>(_first_y, ctx);
    }
    remove_list<&EntityCtx::_next_z, &EntityCtx::_prev_z>(_first_z, ctx);

    // 是否需要返回关注自己离开场景的实体列表
    EntityVector *interest_me = ctx->_interest_me;
    if (list)
    {
        list->insert(list->end(), interest_me->begin(), interest_me->end());
    }

    del_entity_ctx(ctx);

    return 0;
}

int32_t ListAOI::update_entity(EntityId id, int32_t x, int32_t y, int32_t z,
                               EntityVector *list, EntityVector *list_in,
                               EntityVector *list_out)
{
    return 0;
}
