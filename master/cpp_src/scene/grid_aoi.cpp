#include "grid_aoi.h"
#include "scene_include.h"
#include "../system/static_global.h"

// 暂定格子数最大为256，一个格子坐标x占高8位，y占低8位构成一个索引
#define INDEX_BIT 8
#define MAKE_INDEX(x,y) ((int32)x << INDEX_BIT) + y

object_pool< grid_aoi::entity_ctx > grid_aoi::_ctx_pool(10240,1024);
object_pool< grid_aoi::entity_vector_t > grid_aoi::_vector_pool(10240,1024);

grid_aoi::grid_aoi()
{
    _width = 0; // 场景最大宽度(格子坐标)
    _height = 0; // 场景最大高度(格子坐标)

    _visual_width = 0; // 视野宽度格子数
    _visual_height = 0; // 视野高度格子数

    C_OBJECT_ADD("grid_aoi");
}

grid_aoi::~grid_aoi()
{
    entity_set_t::iterator iter = _entity_set.begin();
    for (;iter != _entity_set.end();iter ++) del_entity_ctx(iter->second);

    map_t< uint32,entity_vector_t* >::iterator viter = _entity_grid.begin();
    for (;viter != _entity_grid.end();viter ++) del_entity_vector(viter->second);

    _entity_set.clear();
    _entity_grid.clear();

    C_OBJECT_DEC("grid_aoi");
}

// 需要实现缓存，太大的直接删除不要丢缓存
void grid_aoi::del_entity_vector(entity_vector_t *list)
{
    _vector_pool.destroy(list,list->size() > 512);
}

grid_aoi::entity_vector_t *grid_aoi::new_entity_vector()
{
    entity_vector_t *vt = _vector_pool.construct();

    vt->clear();
    return vt;
}

void grid_aoi::del_entity_ctx(struct entity_ctx *ctx)
{
    del_entity_vector(ctx->_watch_me);

    _ctx_pool.destroy(ctx);
}

struct grid_aoi::entity_ctx *grid_aoi::new_entity_ctx()
{
    struct entity_ctx *ctx = _ctx_pool.construct();

    ctx->_watch_me = new_entity_vector();

    return ctx;
}

// 设置视野
// @width,@height 格子数
void grid_aoi::set_visual_range(int32 width,int32 height)
{
    _visual_width = width;
    _visual_height = height;
}

// 设置宽高
// @width,@height 像素
int32 grid_aoi::set_size(int32 width,int32 height)
{
    _width = PIX_TO_GRID(width);
    _height = PIX_TO_GRID(height);

    static const int max_grid = 0x01 << INDEX_BIT;
    if (_width > max_grid || _height > max_grid)
    {
        _width = 0;
        _height = 0;
        return -1;
    }

    return 0;
}

/* 获取某一范围内实体
 * 底层这里只支持矩形，如果是其他形状的，上层根据实体位置再筛选即可
 */
int32 grid_aoi::get_entitys(
    entity_vector_t *list,int32 srcx,int32 srcy,int32 destx,int32 desty)
{
    // 4个坐标必须为矩形的对角像素坐标,这里转换为左上角和右下角坐标
    int32 x = srcx;
    int32 y = srcy;
    int32 dx = destx;
    int32 dy = desty;
    if (srcx > destx) { x = destx;dx = srcx; }
    if (srcy > desty) { y = desty;dy = srcy; }

    // 转换为格子坐标
    x = PIX_TO_GRID(x);
    y = PIX_TO_GRID(y);
    dx = PIX_TO_GRID(dx);
    dy = PIX_TO_GRID(dy);

    return raw_get_entitys(list,x,y,dx,dy);
}

// 获取矩形内的实体
int32 grid_aoi::raw_get_entitys(
    entity_vector_t *list,int32 x,int32 y,int32 dx,int32 dy)
{
    // 限制越界
    if (x < 0 || y < 0) return 1;
    if (dx > _width || dy > _height) return 2;

    // 遍历范围内的所有格子
    // 注意坐标是格子的中心坐标，因为要包含当前格子，用<=
    for (int32 ix = x;ix <= dx;ix ++)
    {
        for (int32 iy = y;iy <= dy;iy ++)
        {
            entity_vector_t *grid_list = get_grid_entitys(ix,iy);
            if (!grid_list) continue;

            list->insert(list->end(),grid_list->begin(),grid_list->end());
        }
    }

    return 0;
}

// 获取格子内的实体列表
grid_aoi::entity_vector_t *grid_aoi::get_grid_entitys(int32 x,int32 y)
{
    int32 index = MAKE_INDEX(x,y);

    map_t< uint32,entity_vector_t* >::iterator itr = _entity_grid.find(index);
    if (itr == _entity_grid.end()) return NULL;

    return itr->second;
}

bool grid_aoi::remove_entity_from_vector(
    entity_vector_t *list,const struct entity_ctx *ctx)
{
    entity_vector_t::iterator iter = list->begin();
    for (;iter != list->end();iter ++)
    {
        if (*iter == ctx)
        {
            // 用最后一个元素替换就好，不用移动其他元素
            *iter = list->back();
            list->pop_back();

            return true;
        }
    }

    return false;
}

// 删除格子内实体
bool grid_aoi::remove_grid_entity(int32 x,int32 y,const struct entity_ctx *ctx)
{
    int32 index = MAKE_INDEX(x,y);
    map_t< uint32,entity_vector_t* >::iterator iter = _entity_grid.find(index);
    if (iter == _entity_grid.end() || !iter->second) return false;

    entity_vector_t *grid_list = iter->second;
    bool isDel = remove_entity_from_vector(grid_list,ctx);

    // 这个格子不再有实体就清空
    if (grid_list->empty())
    {
        del_entity_vector(grid_list);
        _entity_grid.erase(iter);
    }

    return isDel;
}

// 插入实体到格子内
void grid_aoi::insert_grid_entity(int32 x,int32 y,struct entity_ctx *ctx)
{
    int32 index = MAKE_INDEX(x,y);

    // https://en.cppreference.com/w/cpp/language/value_initialization
    // 没创建时，grid_list应该是NULL
    entity_vector_t *&grid_list = _entity_grid[index];
    // 注意上面用的是指针的引用
    if (!grid_list) grid_list = new_entity_vector();

    grid_list->push_back(ctx);
}

// 获取实体的ctx
struct grid_aoi::entity_ctx *grid_aoi::get_entity_ctx(entity_id_t id)
{
    entity_set_t::const_iterator itr = _entity_set.find(id);
    if (_entity_set.end() == itr) return NULL;

    return itr->second;
}

// 处理实体退出场景
int32 grid_aoi::exit_entity(entity_id_t id,entity_vector_t *list)
{
    entity_set_t::iterator iter = _entity_set.find(id);
    if (_entity_set.end() == iter) return 1;

    struct entity_ctx *ctx = iter->second;
    _entity_set.erase(iter);

    if (!ctx) return 2;
    bool isOk = remove_grid_entity(ctx->_pos_x,ctx->_pos_y,ctx);

    // 是否需要返回关注自己离开场景的实体列表
    entity_vector_t *watch_me= ctx->_watch_me;
    if (list && watch_me && !watch_me->empty())
    {
        list->insert(list->end(),watch_me->begin(),watch_me->end());
    }

    // 从别人的watch_me列表删除
    // 自己关注event才有可能出现在别人的watch列表中
    if (ctx->_event)
    {
        int32 x = 0,y = 0,dx = 0,dy = 0;
        get_visual_range(x,y,dx,dy,ctx->_pos_x,ctx->_pos_y);

        // 把自己的列表清空，这样从自己列表中删除时就不用循环了
        watch_me->clear();
        entity_exit_range(ctx,x,y,dx,dy);
    }

    del_entity_ctx(ctx);

    return isOk ? 0 : -1;
}

// 处理实体退出某个范围
void grid_aoi::entity_exit_range(struct entity_ctx *ctx,
    int32 x,int32 y,int32 dx,int32 dy,entity_vector_t *list)
{
     entity_vector_t *watch_list = new_entity_vector();

    raw_get_entitys(watch_list,x,y,dx,dy);
    entity_vector_t::iterator iter = watch_list->begin();
    for (;iter != watch_list->end();iter ++)
    {
        // 从别人的watch_me列表删除自己，并且从自己的watch_me列表中删除别人
        struct entity_ctx *other = *iter;
        // 自己关注event才有可能出现在别人的watch列表中
        if (ctx->_event) remove_entity_from_vector(other->_watch_me,ctx);
        if (other->_event)
        {
            remove_entity_from_vector(ctx->_watch_me,other);
            if (list) list->push_back(other);
        }
    }

    del_entity_vector(watch_list);
}

// 处理实体进入场景
int32 grid_aoi::enter_entity(
    entity_id_t id,int32 x,int32 y,uint8 type,uint8 event,entity_vector_t *list)
{
    // 检测坐标
    int32 gx = PIX_TO_GRID(x);
    int32 gy = PIX_TO_GRID(y);
    if (gx < 0 || gy < 0 || gx > _width || gy > _height) return 1;

    // 防止重复进入场景
    std::pair< entity_set_t::iterator,bool > ret;
    ret = _entity_set.insert(
        std::pair<entity_id_t,struct entity_ctx*>(id,NULL));
    if (false == ret.second) return 2;

    struct entity_ctx *ctx = new_entity_ctx();
    ret.first->second = ctx;

    ctx->_id = id;
    ctx->_pos_x = gx;
    ctx->_pos_y = gy;
    ctx->_type = type;
    ctx->_event = event;

    // 先取事件列表，这样就不会包含自己
    int32 vx = 0,vy = 0,vdx = 0,vdy = 0;
    get_visual_range(vx,vy,vdx,vdy,gx,gy);
    entity_enter_range(ctx,vx,vy,vdx,vdy,list);

    insert_grid_entity(gx,gy,ctx); // 插入到格子内

    return 0;
}

// 处理实体进入某个范围
void grid_aoi::entity_enter_range(struct entity_ctx *ctx,
    int32 x,int32 y,int32 dx,int32 dy,entity_vector_t *list)
{
    entity_vector_t *watch_list = new_entity_vector();
    raw_get_entitys(watch_list,x,y,dx,dy);

    entity_vector_t *watch_me = ctx->_watch_me;
    entity_vector_t::iterator iter = watch_list->begin();
    for (;iter != watch_list->end();iter ++)
    {
        struct entity_ctx *other = *iter;
        // 把自己加到别人的watch
        if (ctx->_event) other->_watch_me->push_back(ctx);
        // 把别人加到自己的watch
        if (other->_event)
        {
            watch_me->push_back(other);

            // 返回需要触发aoi事件的实体
            if (list) list->push_back(other);
        }
    }

    del_entity_vector(watch_list);
}

/* 判断两个位置视野交集
 * @x,y,dx,dy:矩形区域视野的对角坐标
 * @pos_x,pos_y:实体旧位置坐标
 */
// 判断视野范围
void grid_aoi::get_visual_range(
    int32 &x,int32 &y,int32 &dx,int32 &dy,int32 pos_x,int32 pos_y)
{
    // 以pos为中心，构造一个矩形视野
    x = pos_x - _visual_width;
    y = pos_y - _visual_height;
    dx = pos_x + _visual_width;
    dy = pos_y + _visual_height;

    // 处理边界
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (dx > _width) dx = _width;
    if (dy > _height) dy = _height;
}
/* 更新实体位置
 * @list_in:接收实体进入的实体列表
 * @list_out:接收实体消失的实体列表
 * @list:接收实体更新的实体列表
 */
int32 grid_aoi::update_entity(entity_id_t id,
        int32 x,int32 y,entity_vector_t *list,
        entity_vector_t *list_in,entity_vector_t *list_out)
{
    // 检测坐标
    int32 gx = PIX_TO_GRID(x);
    int32 gy = PIX_TO_GRID(y);
    if (gx < 0 || gy < 0 || gx > _width || gy > _height) return 1;

    struct entity_ctx *ctx = get_entity_ctx(id);
    if (!ctx) return 2;

    // 在一个格子内移动不用处理
    if (gx == ctx->_pos_x && gy == ctx->_pos_y) return 0;

    // 获取旧视野
    int32 old_x = 0,old_y = 0,old_dx = 0,old_dy = 0;
    get_visual_range(old_x,old_y,old_dx,old_dy,ctx->_pos_x,ctx->_pos_y);

    // 获取新视野
    int32 new_x = 0,new_y = 0,new_dx = 0,new_dy = 0;
    get_visual_range(new_x,new_y,new_dx,new_dy,gx,gy);

    /* 求矩形交集 intersection
     * 1. 分别取两个矩形左上角坐标中x、y最大值作为交集矩形的左上角的坐标
     * 2. 分别取两个矩形的右下角坐标x、y最小值作为交集矩形的右下角坐标
     * 3. 判断交集矩形的左上角坐标是否在右下角坐标的左上方。如果否则没有交集
     */
    bool intersection = true;
    int32 it_x = MATH_MAX(old_x,new_x);
    int32 it_y = MATH_MAX(old_y,new_y);
    int32 it_dx = MATH_MIN(old_dx,new_dx);
    int32 it_dy = MATH_MIN(old_dy,new_dy);
    if (it_x > it_dx || it_y > it_dy) intersection = false;

    // 从旧格子退出
    bool exitOk = remove_grid_entity(ctx->_pos_x,ctx->_pos_y,ctx);

    // 由于事件列表不包含自己，退出格子后先取列表再进入新格子

    // 交集区域内玩家，触发更新事件
    // 旧视野区域，触发退出
    // 新视野区域，触发进入
    if (!intersection)
    {
        ctx->_watch_me->clear(); // 把列表清空，退出时减少查找时间
        entity_exit_range(ctx,old_x,old_y,old_dx,old_dy,list_out);
        entity_enter_range(ctx,new_x,new_y,new_dx,new_dy,list_in);

        goto INSETION;// 进入新格子
        return -1;
    }

    raw_get_entitys(list,it_x,it_y,it_dx,it_dy);

    for (int32 ix = old_x;ix <= old_dx;ix ++)
    {
        // 排除交集区域
        // 因为视野这个矩形不可以旋转，所以交集区域总在矩形的4个角上
        // 按x轴筛选，那y轴就有几种情况：1无效，2取上半段，3取下段
        int32 iy = old_y;
        int32 idy = old_dy;
        if (ix >= it_x && ix <= it_dx)
        {
            if (old_dy > it_dy) // 下段
            {
                iy = it_dy + 1;
                idy = old_dy;
            }
            else if (old_y < it_y) // 上段
            {
                iy = old_y;
                idy = it_y - 1;
            }
            else
            {
                continue; // 无效
            }
        }

        assert("rectangle difference fail",iy <= idy);
        entity_exit_range(ctx,ix,iy,ix,idy,list_out);
    }

    for (int32 ix = new_x;ix <= new_dx;ix ++)
    {
        int32 iy = new_y;
        int32 idy = new_dy;
        if (ix >= it_x && ix <= it_dx)
        {
            if (new_dy > it_dy) // 下段
            {
                iy = it_dy + 1;
                idy = new_dy;
            }
            else if (new_y < it_y) // 上段
            {
                iy = new_y;
                idy = it_y - 1;
            }
            else
            {
                continue; // 无效
            }
        }

        assert("rectangle difference fail",iy <= idy);
        entity_enter_range(ctx,ix,iy,ix,idy,list_in);
    }

INSETION:
    // 进入新格子
    ctx->_pos_x = gx;
    ctx->_pos_y = gy;
    insert_grid_entity(gx,gy,ctx);

    return exitOk ? 0 : -1;
}
