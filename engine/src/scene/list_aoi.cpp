#include "list_aoi.h"

ListAOI::ListAOI()
{
    _first_x = nullptr;
    _last_x  = nullptr;
    _first_y = nullptr;
    _last_y  = nullptr;
    _first_z = nullptr;
    _last_z  = nullptr;

    _use_y   = true;
    _max_fov = 0;
}

ListAOI::~ListAOI()
{
    for (auto &iter : _entity_set) del_entity_ctx(iter.second);
}

bool ListAOI::enter_entity(EntityId id, int32_t x, int32_t y, int32_t z,
                           int32_t fov, uint8_t mask, EntityVector *list)
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

    ctx->_id    = id;
    ctx->_mask  = mask;
    ctx->_pos_x = x;
    ctx->_pos_y = y;
    ctx->_pos_z = z;
    ctx->_fov   = fov;

    add_fov(fov); // 记录视野

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
    int32_t prev_fov = x - _max_fov;
    int32_t next_fov = x + _max_fov;
    EntityCtx *prev  = ctx->_prev_x;
    while (prev && prev->_pos_x >= prev_fov)
    {
        if (in_fov(prev, x, y, z))
        {
            ctx->_interest_me->emplace_back(prev);
        }
        if (list) list->emplace_back(prev);
        prev = prev->_prev_x;
    }
    EntityCtx *next = ctx->_next_x;
    while (next && next->_pos_x <= next_fov)
    {
        if (in_fov(next, x, y, z))
        {
            ctx->_interest_me->emplace_back(next);
        }
        if (list) list->emplace_back(next);
        next = next->_next_x;
    }
    return true;
}

void ListAOI::add_fov(int32_t fov)
{
    if (!fov) return;

    for (auto iter = _fov_ref.begin(); iter != _fov_ref.end(); ++iter)
    {
        if (iter->_fov == fov)
        {
            iter->_ref++;
            return;
        }
        if (iter->_fov > fov)
        {
            _fov_ref.emplace(iter, fov, 1);
            return;
        }
    }

    _max_fov = fov;
    _fov_ref.emplace_back(fov, 1);
}

void ListAOI::dec_fov(int32_t fov)
{
    if (!fov) return;

    for (auto iter = _fov_ref.begin(); iter != _fov_ref.end(); ++iter)
    {
        if (iter->_fov != fov) continue;

        iter->_ref--;
        if (0 == iter->_ref)
        {
            _fov_ref.erase(iter);
            if (fov == _max_fov)
            {
                _max_fov = _fov_ref.empty() ? 0 : _fov_ref.back()._fov;
            }
        }
        return;
    }

    ERROR("%s no such fov found: %d", __FUNCTION__, fov);
}
