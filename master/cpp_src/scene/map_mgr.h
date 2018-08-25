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
    class grid_map *get_map(int32 id) const;
private:
    map_t<int,class grid_map> _map_set; // 所有地图数据集合
};

#endif /* __MAP_MGR_H__ */
