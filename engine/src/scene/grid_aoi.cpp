#include <cmath>

#include "grid_aoi.h"
#include "../system/static_global.h"

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

    for (int i = 0; i < _width * _height; i++)
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

/**
 * 获取某一范围内实体
 * 底层这里只支持矩形，如果是其他形状的，上层根据实体位置再筛选即可
 * 传入的坐标均为像素坐标
 */
int32_t GridAOI::get_entity(EntityVector *list, int32_t srcx, int32_t srcy,
                            int32_t destx, int32_t desty)
{
    // 4个坐标必须为矩形的对角像素坐标,这里转换为左上角和右下角坐标
    int32_t x  = srcx;
    int32_t y  = srcy;
    int32_t dx = destx;
    int32_t dy = desty;
    if (srcx > destx)
    {
        x  = destx;
        dx = srcx;
    }
    if (srcy > desty)
    {
        y  = desty;
        dy = srcy;
    }

    // 转换为格子坐标
    x  = x / _pix_grid;
    y  = y / _pix_grid;
    dx = dx / _pix_grid;
    dy = dy / _pix_grid;

    if (!valid_pos(x, y, dx, dy)) return -1;

    return get_range_entity(list, x, y, dx, dy);
}

void GridAOI::each_range_entity(int32_t x, int32_t y, int32_t dx, int32_t dy,
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

// 获取矩形内的实体
int32_t GridAOI::get_range_entity(EntityVector *list, int32_t x, int32_t y,
                                  int32_t dx, int32_t dy)
{
    each_range_entity(x, y, dx, dy,
                      [list](EntityCtx *ctx) { list->emplace_back(ctx); });

    return 0;
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
    // TODO 当列表为空时，这里是否需要把数组从_entity_grid移除
    // 如果是持久的副本，应该还会用到，如果是临时副本，一会儿就销毁了，好像移除也没多大作用
    EntityVector *list = _entity_grid[x + _width * y];
    return list ? remove_entity_from_vector(list, ctx) : false;
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

    if (!ctx) return 2;
    bool ok = remove_grid_entity(ctx->_pos_x, ctx->_pos_y, ctx);

    // 是否需要返回关注自己离开场景的实体列表
    EntityVector *interest_me = ctx->_interest_me;
    if (list && interest_me)
    {
        list->insert(list->end(), interest_me->begin(), interest_me->end());
    }

    // 从别人的interest_me列表删除
    // 自己关注event才有可能出现在别人的interest列表中
    if (ctx->_mask)
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

// 处理实体退出某个范围
void GridAOI::entity_exit_range(struct EntityCtx *ctx, int32_t x, int32_t y,
                                int32_t dx, int32_t dy, EntityVector *list)
{
    uint8_t mask = ctx->_mask;

    each_range_entity(x, y, dx, dy, [list, ctx, mask](EntityCtx *other) {
        if (is_interest(mask, other))
        {
            GridAOI::remove_entity_from_vector(ctx->_interest_me, other);
            GridAOI::remove_entity_from_vector(other->_interest_me, ctx);
            if (list) list->push_back(other);
        }
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
    uint8_t mask              = ctx->_mask;
    EntityVector *interest_me = ctx->_interest_me;
    each_range_entity(x, y, dx, dy,
                      [list, ctx, interest_me, mask](EntityCtx *other) {
                          // 加入到双方的interest列表
                          if (GridAOI::is_interest(mask, other))
                          {
                              interest_me->push_back(other);
                              other->_interest_me->push_back(ctx);
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
                               EntityVector *list, EntityVector *list_in,
                               EntityVector *list_out)
{
    // 检测坐标
    int32_t gx = x / _pix_grid;
    int32_t gy = y / _pix_grid;
    if (!valid_pos(gx, gy, gx, gy)) return -1;

    struct EntityCtx *ctx = get_entity_ctx(id);
    if (!ctx) return -2;

    // 在一个格子内移动，只需要返回关注该玩家的列表即可
    // AOI这边其实不用做任何处理，但其他玩家要看得到他移动
    // TODO 如果格子太小，或者不需要很精确显示位置的，这里都可以不处理
    if (gx == ctx->_pos_x && gy == ctx->_pos_y)
    {
        list->insert(list->end(), ctx->_interest_me->begin(),
                     ctx->_interest_me->end());
        return 1;
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

    if (list) get_range_entity(list, it_x, it_y, it_dx, it_dy);

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

        ASSERT(iy <= idy, "rectangle difference fail");
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

        ASSERT(iy <= idy, "rectangle difference fail");
        entity_enter_range(ctx, ix, iy, ix, idy, list_in);
    }

INSETION:
    // 进入新格子
    ctx->_pos_x = gx;
    ctx->_pos_y = gy;
    insert_grid_entity(gx, gy, ctx);

    return ok ? 0 : -2;
}
