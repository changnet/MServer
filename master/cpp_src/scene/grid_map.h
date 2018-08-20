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
    // 加载单个地图文件
    bool load_file(const char *path);
private:

};

#endif /* __GRID_MAP_H__ */
