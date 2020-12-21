#include "list_aoi.h"

ListAOI::ListAOI()
{
    _first_x = nullptr;
    _last_x  = nullptr;
    _first_y = nullptr;
    _last_y  = nullptr;
    _first_z = nullptr;
    _last_z  = nullptr;

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

void ListAOI::each_range_entity(EntityCtx *ctx, int32_t visual,
                                std::function<void(EntityCtx *)> &&func)
{
    // 实体同时存在三轴链表上，只需要遍历其中一个链表即可

    // 往链表左边遍历
    int32_t prev_visual = ctx->_pos_x - visual;
    EntityCtx *prev     = ctx->_prev_x;
    while (prev && prev->_pos_x >= prev_visual)
    {
        func(prev);
        prev = prev->_prev_x;
    }

    // 往链表右边遍历
    int32_t next_visual = ctx->_pos_x + visual;
    EntityCtx *next     = ctx->_next_x;
    while (next && next->_pos_x <= next_visual)
    {
        func(next);
        next = next->_next_x;
    }
}

bool ListAOI::enter_entity(EntityId id, int32_t x, int32_t y, int32_t z,
                           int32_t visual, uint8_t mask, EntityVector *list)
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
    // 记录视野。视野是用来处理interest列表的，如果实体不需要感知其他实体，那它的视野就不需要处理
    if (interest) add_visual(visual);

    // 分别插入三轴链表
    insert_list<&EntityCtx::_pos_x, &EntityCtx::_next_x, &EntityCtx::_prev_x>(
        _first_x, ctx);
    if (_use_y)
    {
        insert_list<&EntityCtx::_pos_y, &EntityCtx::_next_y, &EntityCtx::_prev_y>(
            _first_x, ctx);
    }
    insert_list<&EntityCtx::_pos_z, &EntityCtx::_next_z, &EntityCtx::_prev_z>(
        _first_x, ctx);

    // 建立interest me列表
    each_range_entity(
        ctx, _max_visual, [this, ctx, list, interest, x, y, z](EntityCtx *other) {
            if ((other->_mask & INTEREST) && in_visual(other, x, y, z))
            {
                ctx->_interest_me->emplace_back(other);
            }
            if (interest
                && in_visual(ctx, other->_pos_x, other->_pos_y, other->_pos_z))
            {
                other->_interest_me->push_back(ctx);
            }
            if (list) list->emplace_back(other);
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

        dec_visual(ctx->_visual);
    }

    // 从三轴中删除自己
    remove_list<&EntityCtx::_next_x, &EntityCtx::_prev_x>(_first_x, ctx);
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

void ListAOI::add_visual(int32_t visual)
{
    if (!visual) return;

    for (auto iter = _visual_ref.begin(); iter != _visual_ref.end(); ++iter)
    {
        if (iter->_visual == visual)
        {
            iter->_ref++;
            return;
        }
        if (iter->_visual > visual)
        {
            _visual_ref.emplace(iter, visual, 1);
            return;
        }
    }

    _max_visual = visual;
    _visual_ref.emplace_back(visual, 1);
}

void ListAOI::dec_visual(int32_t visual)
{
    if (!visual) return;

    for (auto iter = _visual_ref.begin(); iter != _visual_ref.end(); ++iter)
    {
        if (iter->_visual != visual) continue;

        iter->_ref--;
        if (0 == iter->_ref)
        {
            _visual_ref.erase(iter);
            if (visual == _max_visual)
            {
                _max_visual =
                    _visual_ref.empty() ? 0 : _visual_ref.back()._visual;
            }
        }
        return;
    }

    ERROR("%s no such visual found: %d", __FUNCTION__, visual);
}
