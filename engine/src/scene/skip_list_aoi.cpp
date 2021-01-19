#include <cmath>
#include "skip_list_aoi.hpp"

void SkipListAOI::EntityCtx::reset()
{
    _mask  = 0;
    _pos_x = 0;
    _pos_y = 0;
    _pos_z = 0;

    _id          = 0;
    _visual      = 0;
    _interest_me = nullptr;
}

SkipListAOI::SkipListAOI()
{
    _index      = 0;
    _max_visual = 0;
}

SkipListAOI::~SkipListAOI()
{
    _list.clear();
    _indexer.clear();
}

struct SkipListAOI::EntityCtx *SkipListAOI::get_entity_ctx(EntityId id)
{
    EntitySet::const_iterator itr = _entity_set.find(id);
    if (_entity_set.end() == itr) return nullptr;

    return itr->second;
}

void SkipListAOI::add_visual(int32_t visual)
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

void SkipListAOI::del_visual(int32_t visual)
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

void SkipListAOI::set_index(int32_t index, int32_t max_x)
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

void SkipListAOI::each_range_entity(const EntityCtx *ctx, int32_t prev_visual,
                                    int32_t next_visual,
                                    std::function<void(EntityCtx *ctx)> &&func)
{
    // 往链表左边遍历(注意这个for循环不会遍历begin本身，但由于第一个节点必须是索引，这里
    // 刚好不需要遍历)
    auto prev = ctx->_iter;
    for (--prev; prev != _list.begin() && (*prev)->_pos_x >= prev_visual; --prev)
    {
        if ((*prev)->_id) func(*prev);
    }

    // 往链表右边遍历
    auto next = ctx->_iter;
    for (++next; next != _list.end() && (*next)->_pos_x <= next_visual; ++next)
    {
        if ((*next)->_id) func(*next);
    }
}

void SkipListAOI::each_range_entity(const EntityCtx *ctx, int32_t visual,
                                    std::function<void(EntityCtx *)> &&func)
{
    each_range_entity(ctx, ctx->_pos_x - visual, ctx->_pos_x + visual,
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
        if (ctx->_mask & INTEREST)
        {
            other->_interest_me->push_back(ctx);
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
        if (ctx->_mask & INTEREST)
        {
            remove_entity_from_vector(other->_interest_me, ctx);
        }

        // "别人离开我的视野" "我离开别人的视野" 取决于ctx与other的位置
        if (list_out) list_out->emplace_back(me ? other : ctx);
    }
}

bool SkipListAOI::enter_entity(EntityId id, int32_t x, int32_t y, int32_t z,
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
    ctx->_iter = _list.insert(iter, ctx);

    each_range_entity(ctx, _max_visual,
                      [this, ctx, list_me_in, list_other_in](EntityCtx *other) {
                          // 自己出现在别人的视野
                          on_enter_range(ctx, other, list_me_in, false);
                          // 别人出现在自己的视野
                          on_enter_range(other, ctx, list_other_in, false);
                      });

    return true;
}

int32_t SkipListAOI::exit_entity(EntityId id, EntityVector *list)
{
    EntitySet::iterator iter = _entity_set.find(id);
    if (_entity_set.end() == iter) return 1;

    EntityCtx *ctx = iter->second;
    _entity_set.erase(iter);

    // 从别人的interest_me列表删除自己
    if (ctx->_visual > 0 && ctx->_mask & INTEREST)
    {
        each_range_entity(ctx, ctx->_visual, [this, ctx](EntityCtx *other) {
            on_exit_range(ctx, other, nullptr, true);
        });
    }

    // 从链表中删除自己
    _list.erase(ctx->_iter);

    // 是否需要返回关注自己离开场景的实体列表
    if (list)
    {
        EntityVector *interest_me = ctx->_interest_me;
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
        if (ctx->_mask & INTEREST)
        {
            other->_interest_me->emplace_back(ctx);
        }
        if (list_in) list_in->emplace_back(me ? other : ctx);
    }
    else if (!is_in && was_in)
    {
        // 离开我视野
        if (ctx->_mask & INTEREST)
        {
            remove_entity_from_vector(other->_interest_me, ctx);
        }
        if (list_out) list_out->emplace_back(me ? other : ctx);
    }
}

int32_t SkipListAOI::update_entity(EntityId id, int32_t x, int32_t y, int32_t z,
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

    int32_t old_x = ctx->_pos_x;
    int32_t old_y = ctx->_pos_y;
    int32_t old_z = ctx->_pos_z;

    ctx->_pos_x = x;
    ctx->_pos_y = y;
    ctx->_pos_z = z;

    // 一般来说，update的时候是因为玩家移动，变化比较小，因此不需要用索引来定位，直接在链表中移动
    auto iter           = ctx->_iter;
    int32_t prev_visual = 0;
    int32_t next_visual = 0;
    if (x > old_x)
    {
        next_visual = x + _max_visual;
        prev_visual = old_x - _max_visual;

        while (iter != _list.begin() && *ctx > **iter) --iter;
    }
    else
    {
        prev_visual = x - _max_visual;
        next_visual = old_x + _max_visual;

        while (iter != _list.end() && **iter < *ctx) ++iter;
    }

    // 在iter之前插入当前实体
    if (iter != ctx->_iter)
    {
        _list.erase(ctx->_iter);
        ctx->_iter = _list.insert(iter, ctx);
    }

    each_range_entity(
        ctx, prev_visual, next_visual,
        [this, ctx, old_x, old_y, old_z, list_me_in, list_other_in, list_me_out,
         list_other_out](EntityCtx *other) {
            bool is_in_me  = in_visual(ctx, other);
            bool was_in_me = in_visual(other, old_x, old_y, old_z, ctx->_visual);
            on_change_range(ctx, other, is_in_me, was_in_me, list_me_in,
                            list_me_out, true);

            bool is_in_other = in_visual(other, ctx);
            bool was_in_other =
                in_visual(other, old_x, old_y, old_z, ctx->_visual);
            on_change_range(other, ctx, is_in_other, was_in_other,
                            list_other_in, list_other_out, false);
        });

    return 0;
}

int32_t SkipListAOI::update_visual(EntityId id, int32_t visual,
                                   EntityVector *list_me_in,
                                   EntityVector *list_me_out)
{
    struct EntityCtx *ctx = get_entity_ctx(id);
    if (!ctx)
    {
        ERROR("%s no ctx found: " FMT64d, __FUNCTION__, id);
        return -1;
    }
    assert(visual != ctx->_visual && ctx->_mask & INTEREST);

    int32_t old_visual = ctx->_visual;

    del_visual(old_visual);
    ctx->_visual = visual;
    add_visual(visual);

    // 不用取_max_visual，因为只是自己的视野变化了，与其他人无关
    int32_t max_visual  = std::max(old_visual, visual);
    int32_t prev_visual = ctx->_pos_x - max_visual;
    int32_t next_visual = ctx->_pos_x + max_visual;
    each_range_entity(
        ctx, prev_visual, next_visual,
        [this, ctx, old_visual, list_me_in, list_me_out](EntityCtx *other) {
            bool is_in_me  = in_visual(ctx, other);
            bool was_in_me = in_visual(ctx, other->_pos_x, other->_pos_y,
                                       other->_pos_z, old_visual);
            on_change_range(ctx, other, is_in_me, was_in_me, list_me_in,
                            list_me_out, true);
        });

    return 0;
}

void SkipListAOI::each_entity(std::function<bool(EntityCtx *)> &&func)
{
    for (auto x : _list)
    {
        if (x->_id) func(x);
    }
}

void SkipListAOI::dump() const
{
    PRINTF("================ >>");
    for (auto x : _list)
    {
        PRINTF("(id = " FMT64d ", x = %5d, y = %5d, z = %5d", x->_id, x->_pos_x,
               x->_pos_y, x->_pos_z);
    }
    PRINTF("================ <<");
}
