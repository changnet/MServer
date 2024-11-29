#include "grid_map.hpp"
#include "system/static_global.hpp"

GridMap::GridMap()
{
    id_       = 0;
    width_    = 0;
    height_   = 0;
    grid_set_ = nullptr;
}

GridMap::~GridMap()
{
    delete[] grid_set_;
}

// 加载单个地图文件
bool GridMap::load_file(const char *path)
{
    return true;
}

// 设置地图信息
bool GridMap::set(int32_t id, int32_t width, int32_t height)
{
    assert(nullptr == grid_set_);

    if (MAX_MAP_GRID < width || MAX_MAP_GRID < height) return false;

    id_       = id;
    width_    = width;
    height_   = height;
    grid_set_ = new int8_t[width_ * height_];

    // 全部初始化为不可行走
    memset(grid_set_, -1, sizeof(int8_t) * width_ * height);

    return true;
}

// 填充地图信息
bool GridMap::fill(int32_t x, int32_t y, int8_t cost)
{
    if (x >= width_ || y >= height_) return false;

    grid_set_[x * height_ + y] = cost;

    return true;
}

// 获取经过这个格子的消耗, < 0 表示不可行
int8_t GridMap::get_pass_cost(int32_t x, int32_t y) const
{
    if (unlikely(x < 0 || x >= width_)) return -1;
    if (unlikely(y < 0 || y >= height_)) return -1;

    return grid_set_[x * height_ + y];
}
