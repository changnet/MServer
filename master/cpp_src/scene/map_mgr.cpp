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
class grid_map *map_mgr::get_map(int32 id)
{
    map_t<int32,class grid_map>::iterator itr = _map_set.find(id);
    if (itr == _map_set.end() ) return NULL;

    return &(itr->second);
}

// 创建一个地图
bool map_mgr::create_map(int32 id,uint16 width,uint16 height)
{
    if ( _map_set.find(id) != _map_set.end() ) return false;

    class grid_map &map = _map_set[id];

    return map.set( id,width,height );
}

// 填充地图数据
bool map_mgr::fill_map(int32 id,uint16 x,uint16 y,int8 cost)
{
    map_t<int32,class grid_map>::iterator itr = _map_set.find(id);
    if (itr == _map_set.end() ) return false;

    return itr->second.fill( x,y,cost );
}