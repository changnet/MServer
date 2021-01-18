#include <cmath>
#include "skip_list_aoi.hpp"

void SkipListAoi::EntityCtx::reset()
{
    _mask  = 0;
    _pos_x = 0;
    _pos_y = 0;
    _pos_z = 0;

    _id          = 0;
    _visual      = 0;
    _interest_me = nullptr;
}

SkipListAoi::SkipListAoi()
{
    _index      = 0;
    _max_visual = 0;
}

SkipListAoi::~SkipListAoi()
{
    _list.clear();
    _indexer.clear();
}

void SkipListAoi::add_visual(int32_t visual)
{
    assert(visual);

    for (auto &x : _visual)
    {
        if (x._visual == visual)
        {
            x._counter++;
            return;
        }
    }

    _visual.emplace_back(Visual{visual, 1});
    if (visual > _max_visual) _max_visual = visual;
}

void SkipListAoi::del_visual(int32_t visual)
{
    assert(visual);

    for (auto &x : _visual)
    {
        if (x._visual != visual) continue;

        x._counter--;
        if (x._counter <= 0)
        {
            x = _visual.back();
            _visual.resize(_visual.size() - 1);
            if (visual == _max_visual)
            {
                _max_visual = _visual.empty() ? 0 : _visual.back()._visual;
            }
        }
        return;
    }

    assert(false);
}

void SkipListAoi::set_index(int32_t index, int32_t max_x)
{
    // 这个只能在aoi初始化时设置一次
    assert(!_index && index && max_x > 0);

    int32_t size = (int32_t)ceil(((double)max_x) / index);

    _index = index;
    _indexer.reserve(size);

    // 在链表中构建特定的节点做为索引
    for (int32_t i = 0; i < size; i++)
    {
        EntityCtx *ctx = new_entity_ctx();

        _list.push_back(ctx);

        ctx->_pos_x = i * index;
        ctx->_iter  = --_list.end();

        _indexer.push_back(ctx);
    }
}

void SkipListAoi::each_range_entity(const EntityCtx *ctx, int32_t visual,
                                    std::function<void(EntityCtx *)> &&func)
{
    // 往链表左边遍历(注意要包含begin，但不包含ctx本身)
    int32_t prev_visual = ctx->_pos_x - visual;
    auto prev           = ctx->_iter;
    while (prev != _list.begin() && (*prev)->_pos_x >= prev_visual)
    {
        --prev;
        if ((*prev)->_id) func(*prev);
    }

    // 往链表右边遍历
    int32_t next_visual = ctx->_pos_x + visual;
    auto next           = ctx->_iter;
    for (next++; next != _list.end() && (*next)->_pos_x <= next_visual; ++next)
    {
        if ((*next)->_id) func(*next);
    }
}

bool SkipListAoi::remove_entity_from_vector(EntityVector *list,
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

void SkipListAoi::on_enter_range(EntityCtx *ctx, EntityCtx *other,
                                 EntityVector *list_in, bool me)
{
    // 这里只是表示进入一个轴，最终是否在视野范围内要判断三轴
    if (in_visual(ctx, other->_pos_x, other->_pos_y, other->_pos_z))
    {
        if (ctx->_mask & INTEREST)
        {
            other->_interest_me->push_back(ctx);
        }

        // "别人进入我的视野" "我进入别人的视野" 取决于ctx与other的位置
        if (list_in) list_in->emplace_back(me ? other : ctx);
    }
}

void SkipListAoi::on_exit_range(EntityCtx *ctx, EntityCtx *other,
                                EntityVector *list_out, bool me)
{
    if (in_visual(ctx, other->_pos_x, other->_pos_y, other->_pos_z))
    {
        if (ctx->_mask & INTEREST)
        {
            remove_entity_from_vector(other->_interest_me, ctx);
        }

        // "别人离开我的视野" "我离开别人的视野" 取决于ctx与other的位置
        if (list_out) list_out->emplace_back(me ? other : ctx);
    }
}

bool SkipListAoi::enter_entity(EntityId id, int32_t x, int32_t y, int32_t z,
                               int32_t visual, uint8_t mask,
                               EntityVector *list_me_in,
                               EntityVector *list_other_in)
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

    ctx->_id     = id;
    ctx->_mask   = mask;
    ctx->_pos_x  = x;
    ctx->_pos_y  = y;
    ctx->_pos_z  = z;
    ctx->_visual = visual;

    if (visual && (mask & INTEREST))
    {
        add_visual(visual);
    }

    // 插入链表
    assert(_index && _indexer.size() > size_t(x / _index));

    auto iter = _indexer[x / _index]->_iter;
    while (iter != _list.end() && **iter < *ctx) ++iter;

    // 在iter之前插入当前实体
    ctx->_iter = _list.insert(--iter, ctx);

    each_range_entity(ctx, _max_visual,
                      [this, ctx, list_me_in, list_other_in](EntityCtx *other) {
                          // 自己出现在别人的视野
                          on_enter_range(ctx, other, list_me_in, false);
                          // 别人出现在自己的视野
                          on_enter_range(other, ctx, list_other_in, false);
                      });

    return true;
}
