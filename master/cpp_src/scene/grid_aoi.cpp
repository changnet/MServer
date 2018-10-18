#include "grid_aoi.h"

#define GRID_PIX 64 // 默认格子边长64像素
#define PIX_TO_GRID(pix) (pix/_grid_pix) // 像素坐标转换为格子坐标，左上角是(0,0)

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
void grid_aoi::set_size(int32 width,int32 height,int32 pix)
{
    _grid_pix = pix > 0 ? pix : GRID_PIX;

    _width = PIX_TO_GRID(width);
    _height = PIX_TO_GRID(height);
}

bool grid_aoi::exit_entity(entity_id_t id)
{
    return true;
}

bool grid_aoi::enter_entity(entity_id_t id,int32 x,int32 y,uint8 type)
{
    return true;
}

bool grid_aoi::update_entity(entity_id_t id,int32 x,int32 y,uint8 type)
{
    return true;
}
