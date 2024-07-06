#include <cmath>

#include "grid_aoi.hpp"
#include "system/static_global.hpp"

GridAOI::GridAOI()
{
    _width  = 0; // 场景最大宽度(格子坐标)
    _height = 0; // 场景最大高度(格子坐标)

    _visual_width  = 0; // 视野宽度格子数
    _visual_height = 0; // 视野高度格子数

    _entity_grid = nullptr;

    C_OBJECT_ADD("grid_aoi");
}

GridAOI::~GridAOI()
{
    for (auto &iter : _entity_set) del_entity_ctx(iter.second);

    _entity_set.clear();

    for (int32_t i = 0; i < _width * _height; i++)
    {
        delete _entity_grid[i];
    }
    delete[] _entity_grid;

    C_OBJECT_DEC("grid_aoi");
}

/**
 * 设置视野，必须先设置场景大小后才能调用此函数
 * @param width 像素
 * @param height 像素
 */
bool GridAOI::set_visual_range(int32_t width, int32_t height)
{
    if (_pix_grid <= 0) return false;

    _visual_width  = (int32_t)std::ceil(((double)width) / _pix_grid);
    _visual_height = (int32_t)std::ceil(((double)height) / _pix_grid);

    return true;
}

void GridAOI::set_size(int32_t width, int32_t height, int32_t pix_grid)
{
    assert(pix_grid > 0 && !_entity_grid);
    _pix_grid = pix_grid;

    _width  = (int32_t)std::ceil(((double)width) / _pix_grid);
    _height = (int32_t)std::ceil(((double)height) / _pix_grid);

    _entity_grid = new EntityVector *[_width * _height];
    memset(_entity_grid, 0, sizeof(EntityVector *) * _width * _height);
}

int32_t GridAOI::each_range_entity(int32_t x, int32_t y, int32_t dx, int32_t dy,
                                   std::function<void(EntityCtx *)> &&func)
{
    // 4个坐标必须为矩形的对角像素坐标,这里转换为左上角和右下角坐标
    if (x > dx || y > dy)
    {
        ELOG("%s invalid pos", __FUNCTION__);
        return -1;
    }

    // 转换为格子坐标
    x  = x / _pix_grid;
    y  = y / _pix_grid;
    dx = dx / _pix_grid;
    dy = dy / _pix_grid;

    if (!valid_pos(x, y, dx, dy)) return -1;

    raw_each_range_entity(x, y, dx, dy,
                          std::forward<std::function<void(EntityCtx *)>>(func));
    return 0;
}

void GridAOI::raw_each_range_entity(int32_t x, int32_t y, int32_t dx, int32_t dy,
                                    std::function<void(EntityCtx *)> &&func)
{
    // 遍历范围内的所有格子
    // 注意坐标是格子的中心坐标，因为要包含当前格子，用<=
    for (int32_t ix = x; ix <= dx; ix++)
    {
        for (int32_t iy = y; iy <= dy; iy++)
        {
            const EntityVector *list = _entity_grid[ix + _width * iy];
            if (list)
            {
                for (auto ctx : *list) func(ctx);
            }
        }
    }
}

bool GridAOI::remove_entity_from_vector(EntityVector *list,
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

bool GridAOI::remove_grid_entity(int32_t x, int32_t y, const struct EntityCtx *ctx)
{
    // 当列表为空时，把数组从_entity_grid移除放到池里复用
    EntityVector *list = _entity_grid[x + _width * y];
    if (!list) return false;

    remove_entity_from_vector(list, ctx);
    if (list->empty())
    {
        _entity_grid[x + _width * y] = nullptr;
        del_entity_vector(list);
    }
    return true;
}

// 插入实体到格子内
void GridAOI::insert_grid_entity(int32_t x, int32_t y, struct EntityCtx *ctx)
{
    EntityVector *list = _entity_grid[x + _width * y];
    if (!list)
    {
        list                         = new_entity_vector();
        _entity_grid[x + _width * y] = list;
    }
    list->push_back(ctx);
}

// 获取实体的ctx
struct GridAOI::EntityCtx *GridAOI::get_entity_ctx(EntityId id)
{
    EntitySet::const_iterator itr = _entity_set.find(id);
    if (_entity_set.end() == itr) return nullptr;

    return itr->second;
}

// 处理实体退出场景
int32_t GridAOI::exit_entity(EntityId id, EntityVector *list)
{
    EntitySet::iterator iter = _entity_set.find(id);
    if (_entity_set.end() == iter) return 1;

    struct EntityCtx *ctx = iter->second;
    _entity_set.erase(iter);

    bool ok = remove_grid_entity(ctx->_pos_x, ctx->_pos_y, ctx);

    // 是否需要返回关注自己离开场景的实体列表
    EntityVector *interest_me = ctx->_interest_me;
    if (list)
    {
        list->insert(list->end(), interest_me->begin(), interest_me->end());
    }

    // 从别人的interest_me列表删除(自己关注event才有可能出现在别人的interest列表中)
    if (ctx->_mask & INTEREST)
    {
        int32_t x = 0, y = 0, dx = 0, dy = 0;
        get_visual_range(x, y, dx, dy, ctx->_pos_x, ctx->_pos_y);

        // 把自己的列表清空，这样从自己列表中删除时就不用循环了
        interest_me->clear();
        entity_exit_range(ctx, x, y, dx, dy);
    }

    del_entity_ctx(ctx);

    return ok ? 0 : -1;
}

void GridAOI::entity_exit_range(struct EntityCtx *ctx, int32_t x, int32_t y,
                                int32_t dx, int32_t dy, EntityVector *list)
{
    raw_each_range_entity(
        x, y, dx, dy,
        [list, ctx](EntityCtx *other)
        {
            if (ctx->_mask & INTEREST)
            {
                GridAOI::remove_entity_from_vector(other->_interest_me, ctx);
            }
            if (other->_mask & INTEREST)
            {
                GridAOI::remove_entity_from_vector(ctx->_interest_me, other);
            }
            if (list) list->push_back(other);
        });
}

// 处理实体进入场景
int32_t GridAOI::enter_entity(EntityId id, int32_t x, int32_t y, uint8_t mask,
                              EntityVector *list)
{
    // 检测坐标
    int32_t gx = x / _pix_grid;
    int32_t gy = y / _pix_grid;
    if (!valid_pos(gx, gy, gx, gy)) return -1;

    // 防止重复进入场景
    auto ret = _entity_set.emplace(id, nullptr);
    if (false == ret.second) return 2;

    struct EntityCtx *ctx = new_entity_ctx();
    ret.first->second     = ctx;

    ctx->_id    = id;
    ctx->_pos_x = gx;
    ctx->_pos_y = gy;
    ctx->_mask  = mask;

    // 先取事件列表，这样就不会包含自己
    int32_t vx = 0, vy = 0, vdx = 0, vdy = 0;
    get_visual_range(vx, vy, vdx, vdy, gx, gy);
    entity_enter_range(ctx, vx, vy, vdx, vdy, list);

    insert_grid_entity(gx, gy, ctx); // 插入到格子内

    return 0;
}

// 处理实体进入某个范围
void GridAOI::entity_enter_range(struct EntityCtx *ctx, int32_t x, int32_t y,
                                 int32_t dx, int32_t dy, EntityVector *list)
{
    raw_each_range_entity(x, y, dx, dy,
                          [list, ctx](EntityCtx *other)
                          {
                              // 自己对其他实体感兴趣，就把自己加到对方列表，这样对方有变化时才会推送数据给自己
                              if (ctx->_mask & INTEREST)
                                  other->_interest_me->push_back(ctx);

                              // 别人对其他实体感兴趣，把别人加到自己的列表，这样自己有变化才会发数据给对方
                              if (other->_mask & INTEREST)
                              {
                                  ctx->_interest_me->push_back(other);
                              }

                              // 无论是否interest，都返回需要触发aoi事件的实体。假如玩家进入场景时，怪物对他不
                              // interest，但需要把怪物的信息发送给玩家，这步由上层筛选
                              if (list) list->push_back(other);
                          });
}

/* 判断两个位置视野交集
 * @x,y,dx,dy:矩形区域视野的对角坐标
 * @pos_x,pos_y:实体旧位置坐标
 */
// 判断视野范围
void GridAOI::get_visual_range(int32_t &x, int32_t &y, int32_t &dx, int32_t &dy,
                               int32_t pos_x, int32_t pos_y)
{
    // 以pos为中心，构造一个矩形视野
    x  = pos_x - _visual_width;
    y  = pos_y - _visual_height;
    dx = pos_x + _visual_width;
    dy = pos_y + _visual_height;

    // 处理边界
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (dx >= _width) dx = _width - 1;
    if (dy >= _height) dy = _height - 1;
}

int32_t GridAOI::update_entity(EntityId id, int32_t x, int32_t y,
                               EntityVector *list_in, EntityVector *list_out)
{
    // 检测坐标
    int32_t gx = x / _pix_grid;
    int32_t gy = y / _pix_grid;
    if (!valid_pos(gx, gy, gx, gy)) return -1;

    struct EntityCtx *ctx = get_entity_ctx(id);
    if (!ctx)
    {
        ELOG("%s no ctx found: " FMT64d, __FUNCTION__, id);
        return -2;
    }

    // 在同一个格子内移动AOI这边其实不用做任何处理
    if (gx == ctx->_pos_x && gy == ctx->_pos_y)
    {
        return 0;
    }

    // 获取旧视野
    int32_t old_x = 0, old_y = 0, old_dx = 0, old_dy = 0;
    get_visual_range(old_x, old_y, old_dx, old_dy, ctx->_pos_x, ctx->_pos_y);

    // 获取新视野
    int32_t new_x = 0, new_y = 0, new_dx = 0, new_dy = 0;
    get_visual_range(new_x, new_y, new_dx, new_dy, gx, gy);

    /* 求矩形交集 intersection
     * 1. 分别取两个矩形左上角坐标中x、y最大值作为交集矩形的左上角的坐标
     * 2. 分别取两个矩形的右下角坐标x、y最小值作为交集矩形的右下角坐标
     * 3. 判断交集矩形的左上角坐标是否在右下角坐标的左上方。如果否则没有交集
     */
    bool intersection = true;
    int32_t it_x      = std::max(old_x, new_x);
    int32_t it_y      = std::max(old_y, new_y);
    int32_t it_dx     = std::min(old_dx, new_dx);
    int32_t it_dy     = std::min(old_dy, new_dy);
    if (it_x > it_dx || it_y > it_dy) intersection = false;

    // 从旧格子退出
    bool ok = remove_grid_entity(ctx->_pos_x, ctx->_pos_y, ctx);

    // 由于事件列表不包含自己，退出格子后先取列表再进入新格子

    // 交集区域内玩家，触发更新事件
    // 旧视野区域，触发退出
    // 新视野区域，触发进入
    if (!intersection)
    {
        ctx->_interest_me->clear(); // 把列表清空，退出时减少查找时间
        entity_exit_range(ctx, old_x, old_y, old_dx, old_dy, list_out);
        entity_enter_range(ctx, new_x, new_y, new_dx, new_dy, list_in);

        goto INSETION; // 进入新格子
        return -1;
    }

    for (int32_t ix = old_x; ix <= old_dx; ix++)
    {
        // 排除交集区域
        // 因为视野这个矩形不可以旋转，所以交集区域总在矩形的4个角上
        // 按x轴筛选，那y轴就有几种情况：1无效，2取上半段，3取下段
        int32_t iy  = old_y;
        int32_t idy = old_dy;
        if (ix >= it_x && ix <= it_dx)
        {
            if (old_dy > it_dy) // 下段
            {
                iy  = it_dy + 1;
                idy = old_dy;
            }
            else if (old_y < it_y) // 上段
            {
                iy  = old_y;
                idy = it_y - 1;
            }
            else
            {
                continue; // 无效
            }
        }

        assert(iy <= idy);
        entity_exit_range(ctx, ix, iy, ix, idy, list_out);
    }

    for (int32_t ix = new_x; ix <= new_dx; ix++)
    {
        int32_t iy  = new_y;
        int32_t idy = new_dy;
        if (ix >= it_x && ix <= it_dx)
        {
            if (new_dy > it_dy) // 下段
            {
                iy  = it_dy + 1;
                idy = new_dy;
            }
            else if (new_y < it_y) // 上段
            {
                iy  = new_y;
                idy = it_y - 1;
            }
            else
            {
                continue; // 无效
            }
        }

        assert(iy <= idy);
        entity_enter_range(ctx, ix, iy, ix, idy, list_in);
    }

INSETION:
    // 进入新格子
    ctx->_pos_x = gx;
    ctx->_pos_y = gy;
    insert_grid_entity(gx, gy, ctx);

    return ok ? 0 : -2;
}
