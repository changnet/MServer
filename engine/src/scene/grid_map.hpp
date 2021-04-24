#pragma once

/* 2d网格地图，就是打格子那种地图
 * 参考tiled编辑器：https://www.mapeditor.org/
 * 2017-07-18 by xzc
 */

#include "../global/global.hpp"

class GridMap
{
public:
    GridMap();
    ~GridMap();

    // 加载单个地图文件
    bool load_file(const char *path);
    // 获取经过这个格子的消耗, < 0 表示不可行
    int8_t get_pass_cost(int32_t x, int32_t y) const;
    // 设置地图信息
    bool set(int32_t id, int32_t width, int32_t height);
    // 填充地图信息
    bool fill(int32_t x, int32_t y, int8_t cost);

    // 获取地图宽高
    int32_t get_width() const { return _width; }
    int32_t get_height() const { return _height; }

private:
    int32_t _id;
    /* 现在的游戏都是精确到像素级别的
     * 但是地形用grid的话没法精确到像素级别，也没必要。因此后端只记录格子坐标
     * 一个格子可以表示64*64像素大小，或者32*32，具体看策划要求
     * 65535*32 = 2097120像素，一般情况下，uint16_t足够大了
     */
    int32_t _width;   // 地图的宽，格子坐标
    int32_t _height;  // 地图的长度，格子坐标
    int8_t *_grid_set; // 格子数据集合
};
