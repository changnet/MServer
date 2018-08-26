#include "ltools.h"
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
    int32 id = luaL_checkinteger(L,1);

    // 返回删除的数量
    lua_pushinteger(L,map_mgr::remove_map(id));
    return 1;
}

int32 lmap_mgr::create_map( lua_State *L ) // 创建一个地图
{
    int32 id     = luaL_checkinteger(L,1);
    int32 width  = luaL_checkinteger(L,2);
    int32 height = luaL_checkinteger(L,3);

    if ( width < 0 || height < 0 ) return 0;

    lua_pushboolean( L,map_mgr::create_map(id,width,height) );
    return 1;
}

int32 lmap_mgr::fill_map( lua_State *L )   // 填充地图数据
{
    int32 id   = luaL_checkinteger(L,1); // 地图id
    int32 x    = luaL_checkinteger(L,2); // 填充的坐标x
    int32 y    = luaL_checkinteger(L,3); // 填充的坐标y
    int32 cost = luaL_checkinteger(L,4); // 该格子的消耗

    lua_pushboolean( L,map_mgr::fill_map(id,x,y,cost) );
    return 1;
}

int32 lmap_mgr::find_path( lua_State *L) // 寻路
{
    int32 id   = luaL_checkinteger(L,1); // 地图id
    int32 x    = luaL_checkinteger(L,2); // 起点坐标x
    int32 y    = luaL_checkinteger(L,3); // 起点坐标x
    int32 dx   = luaL_checkinteger(L,4); // 终点坐标x
    int32 dy   = luaL_checkinteger(L,5); // 终点坐标x

    // 路径放到一个table里，但是C++里不会创建，由脚本那边传入并缓存，防止频繁创建引发gc
    const static int32 tbl_stack = 6;
    lUAL_CHECKTABLE(L,tbl_stack);

    class grid_map *map = get_map( id );
    if ( !map ) return 0;

    if ( !_a_star.search(map,x,y,dx,dy) ) return 0;

    const std::vector<uint16> &path = _a_star.get_path();

    size_t path_sz = path.size();
    // 路径依次存各个点的x,y坐标，应该是成双的
    if ( 0 != path_sz%2 ) return 0;

    int32 tbl_idx = 1;
    // 原来的路径是反向的，这里还原
    for (int32 idx = path_sz - 1;idx > 0;idx -= 2 )
    {
        // x坐标
        lua_pushinteger(L,path[idx - 1]);
        lua_rawseti(L,tbl_stack,tbl_idx++);

        // y坐标
        lua_pushinteger(L,path[idx]);
        lua_rawseti(L,tbl_stack,tbl_idx++);
    }

    // 设置table的n值为路径坐标数
    lua_pushstring(L,"n");
    lua_pushinteger(L,path_sz );
    lua_rawset(L,tbl_stack);

    // 返回路径格子数
    lua_pushinteger(L,path_sz/2);
    return 1;
}
