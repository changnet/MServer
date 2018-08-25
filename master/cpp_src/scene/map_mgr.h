/* 地图管理
 */

#ifndef __MAP_MGR_H__
#define __MAP_MGR_H__

#include "grid_map.h"

class map_mgr
{
public:
    map_mgr();
    virtual ~map_mgr();

    // 加载该目标下所有地图文件
    bool load_path(const char *path,const char *suffix);
    // 获取地图数据
    class grid_map *get_map(int32 id);
    // 删除该地图数据
    int32 remove_map(int32 id) { return _map_set.erase(id); }
    // 创建一个地图
    bool create_map(int32 id,uint16 width,uint16 height);
    // 填充地图数据
    bool fill_map(int32 id,uint16 width,uint16 height,int8 cost);
protected:
    map_t<int32,class grid_map> _map_set; // 所有地图数据集合
};

#endif /* __MAP_MGR_H__ */
