/* 2d网格地图，就是打格子那种地图
 * 参考tiled编辑器：https://www.mapeditor.org/
 * 2017-07-18 by xzc
 */

#ifndef __GRID_MAP_H__
#define __GRID_MAP_H__

#include "../global/global.h"

class grid_map
{
public:
    grid_map();
    ~grid_map();

    // 加载单个地图文件
    bool load_file(const char *path);
    // 获取经过这个格子的消耗, < 0 表示不可行
    int8 get_pass_cost(int32 x,int32 y) const;
    // 设置地图信息
    bool set( int32 id,uint16 width,uint16 height );
    // 填充地图信息
    bool fill( uint16 x,uint16 y,int8 cost );

    // 获取地图宽高
    uint16 get_width() const { return _width; }
    uint16 get_height() const { return _height; }
private:
    int32 _id;
    /* 现在的游戏都是精确到像素级别的
     * 但是地形用grid的话没法精确到像素级别，也没必要。因此后端只记录格子坐标
     * 一个格子可以表示64*64像素大小，或者32*32，具体看策划要求
     * 65535*32 = 2097120像素，一般情况下，uint16_t足够大了
     */
    uint16 _width;  // 地图的宽，格子坐标
    uint16 _height; // 地图的长度，格子坐标
    int8 *_grid_set;// 格子数据集合
};

#endif /* __GRID_MAP_H__ */
