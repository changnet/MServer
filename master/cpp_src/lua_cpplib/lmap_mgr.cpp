#include "lmap_mgr.h"

class lmap_mgr *lmap_mgr::_lmap_mgr = NULL;

void lmap_mgr::uninstance()
{
    delete _lmap_mgr;
    _lmap_mgr = NULL;
}
class lmap_mgr *lmap_mgr::instance()
{
    if ( !_lmap_mgr ) _lmap_mgr = new lmap_mgr( NULL );

    return _lmap_mgr;
}

lmap_mgr::~lmap_mgr()
{
}

lmap_mgr::lmap_mgr( lua_State *L )
{
    assert( "lmap_mgr is singleton",!_lmap_mgr );
}

int32 lmap_mgr::load_path( lua_State *L ) // 加载该目录下所有地图文件
{
    return 0;
}

int32 lmap_mgr::load_file( lua_State *L ) // 加载单个文件
{
    return 0;
}

int32 lmap_mgr::remove_map( lua_State *L ) // 删除一个地图数据
{
    return 0;
}

int32 lmap_mgr::create_map( lua_State *L ) // 创建一个地图
{
    return 0;
}

int32 lmap_mgr::fill_map( lua_State *L )   // 填充地图数据
{
    return 0;
}

int32 lmap_mgr::find_path( lua_State *L) // 寻路
{
    return 0;
}
