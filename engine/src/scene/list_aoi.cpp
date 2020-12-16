#include "list_aoi.h"

ListAOI::ListAOI()
{
    _first_x = nullptr;
    _last_x  = nullptr;
    _first_y = nullptr;
    _last_y  = nullptr;
    _first_z = nullptr;
    _last_z  = nullptr;

    _use_y = true;
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

    add_fov(fov);
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
