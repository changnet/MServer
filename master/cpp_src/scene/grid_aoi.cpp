#include "grid_aoi.h"

#define GRID_PIX 64 // 默认格子边长64像素
#define PIX_TO_GRID(pix) (pix/_grid_pix) // 像素坐标转换为格子坐标，左上角是(0,0)

// 暂定格子数最大为256，一个格子坐标x占高8位，y占低8位构成一个索引
#define INDEX_BIT 8
#define MAKE_INDEX(x,y) ((int32)x << INDEX_BIT) + y

grid_aoi::grid_aoi()
{
    _watch_mask = 0;

    _width = 0; // 场景最大宽度(格子坐标)
    _height = 0; // 场景最大高度(格子坐标)

    _visual_width = 0; // 视野宽度格子数
    _visual_height = 0; // 视野高度格子数

    _grid_pix = GRID_PIX; // 多少像素转换为一个格子边长
}

grid_aoi::~grid_aoi()
{
    _entity_set.clear();
    _entity_grid.clear();
}

// 需要实现缓存，太大的直接删除不要丢缓存
void grid_aoi::del_entity_vector()
{
}

entity_vector_t *grid_aoi::new_entity_vector()
{
    return NULL;
}

void grid_aoi::del_entity_ctx()
{
}

struct entity_ctx *grid_aoi::new_entity_ctx()
{
    return NULL;
}

// 设置需要放入watch_me列表的实体类型
void grid_aoi::set_watch_mask(uint32 mask)
{
    _watch_mask = mask;
}

// 设置视野
// @width,@height 格子数
void grid_aoi::set_visual_range(int32 width,int32 height)
{
    _visual_width = width;
    _visual_height = height;
}

// 设置宽高，格子像素
// @width,@height 像素
bool grid_aoi::set_size(int32 width,int32 height,int32 pix)
{
    _grid_pix = pix > 0 ? pix : GRID_PIX;

    _width = PIX_TO_GRID(width);
    _height = PIX_TO_GRID(height);

    static const max_grid = 0x01 << INDEX_BIT;
    if (_width > max_grid || _height > max_grid)
    {
        _width = 0;
        _height = 0;
        return false;
    }

    return true;
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

    // 限制越界
    if (x < 0 || y < 0) return -1;
    if (dx > _width) dx = _width;
    if (dy > _height) dy = _height;

    // 遍历范围内的所有格子
    for (int32 ix = x;ix < dx;ix ++)
    {
        for (int32 iy = y;iy < dy;iy ++)
        {
            entity_vector_t *grid_list = get_grid_entitys(ix,iy);
            if (!grid_list) continue;

            list->insert(list->end(),grid_list->begin(),grid_list->end());
        }
    }

    return 0;
}

// 获取格子内的实体列表
entity_vector_t *grid_aoi::get_grid_entitys(int32 x,int32 y)
{
    int32 index = MAKE_INDEX(ix,iy);

    map_t< uint32,entity_vector_t* >::iterator itr = _entity_grid.find(index);
    if (itr == _entity_grid.end()) return NULL;

    return itr->second;
}

// 删除格子内实体
bool remove_grid_entity(int32 x,int32 y,entity_id_t id)
{
    int32 index = MAKE_INDEX(x,y);
    map_t< uint32,entity_vector_t* >::iterator itr = _entity_grid.find(index);
    if (itr == _entity_grid.end() || !itr->second) return false;

    entity_vector_t *grid_list = itr->second;
    entity_vector_t::iterator viter = grid_list->begin();
    for (;viter != grid_list->end();viter ++)
    {
        if (*viter == id)
        {
            // 用最后一个元素替换就好，不用移动其他元素
            *viter = grid_list->back();
            grid_list->pop_back();

            // 这个格子不再有实体就清空
            if (grid_list.empty())
            {
                del_entity_vector(grid_list);
                _entity_grid.remove(itr);
            }
            return true;
        }
    }

    return false;
}

// 获取实体的ctx
const struct entity_ctx *grid_aoi::get_entity_ctx(entity_id_t id) const
{
    entity_set_t::const_iterator itr = _entity_set.find(id);
    if (_entity_set.end() == itr) return NULL;

    return itr->second;
}

// 处理实体退出场景
bool grid_aoi::exit_entity(entity_id_t id,entity_vector_t *list)
{
    entity_set_t::iterator itr = _entity_set.find(id);
    if (_entity_set.end() == itr) return false;

    struct entity_ctx *ctx = itr->second;
    _entity_set.remove(itr);

    if (!ctx) return false;
    bool isOk = remove_grid_entity(ctx->_pos_x,ctx->_pos_y,id);

    // 是否需要返回关注自己离开场景的实体列表
    entity_vector_t *watch_me= ctx->_watch_me;
    if (list && watch_me && !watch_me->empty())
    {
        list->insert(list->end(),watch_me->begin(),watch_me->end());
    }

    del_entity_ctx(ctx);

    return isOk;
}

// 处理实体进入场景
bool grid_aoi::enter_entity(entity_id_t id,int32 x,int32 y,uint8 type)
{
    std::pair< entity_set_t::iterator,bool > ret;
    ret = _entity_set.insert(
        std::pair<entity_id_t,struct entity_ctx*>(id,NULL));
    if (false == ret.second) return false;

    struct entity_ctx *ctx = new_entity_ctx();
    ret.first->second = ctx;

    return true;
}

bool grid_aoi::update_entity(entity_id_t id,int32 x,int32 y,uint8 type)
{
    return true;
}
