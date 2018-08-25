#include "map_mgr.h"

map_mgr::map_mgr()
{

}

map_mgr::~map_mgr()
{
    _map_set.clear();
}

// 加载该目标下所有地图文件
bool map_mgr::load_path(const char *path,const char *suffix)
{
    return true;
}
// 获取地图数据
class grid_map *map_mgr::get_map(int id) const
{
    return NULL;
}

