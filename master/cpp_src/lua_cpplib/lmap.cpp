#include "lmap.h"
#include "../scene/scene_include.h"

lmap::~lmap()
{
}

lmap::lmap( lua_State *L )
{
}

int32 lmap::load( lua_State *L ) // 加载地图数据
{
    // TODO: 地图数据格式定下来才能做
    return 0;
}

int32 lmap::set( lua_State *L ) // 设置地图信息(用于动态创建地图)
{
    int32 id     = luaL_checkinteger(L,1);
    int32 width  = luaL_checkinteger(L,2);
    int32 height = luaL_checkinteger(L,3);

    if ( width < 0 || height < 0 ) return 0;

    bool ok = grid_map::set( id,width,height );

    lua_pushboolean( L,ok );
    return 1;
}

int32 lmap::fill( lua_State *L ) // (用于动态创建地图)
{
    int32 x    = luaL_checkinteger(L,1); // 填充的坐标x
    int32 y    = luaL_checkinteger(L,2); // 填充的坐标y
    int32 cost = luaL_checkinteger(L,3); // 该格子的消耗

    bool ok = grid_map::fill( x,y,cost );

    lua_pushboolean( L,ok );
    return 1;
}

int32 lmap::get_size( lua_State *L )   // 获取地图宽高
{
    int32 width = grid_map::get_width();
    int32 height = grid_map::get_height();

    lua_pushinteger( L,width );
    lua_pushinteger( L,height );

    return 2;
}

int32 lmap::fork( lua_State *L ) // 复制一份地图(用于动态修改地图数据)
{
    // TODO:以后可能会加上区域属性，一些属性需要动态修改
    // 这时候我们可以复制一份地图数据，而不会直接修改基础配置数据

    class lmap** udata = (class lmap**)luaL_checkudata( L, 1, "Map" );

    class grid_map *map = *udata;
    if ( !map ) return 0;

    // TODO:暂时没有对应的数据来做

    return 0;
}

int32 lmap::get_pass_cost( lua_State *L ) // 获取通过某个格子的消耗
{
    int32 x = luaL_checkinteger(L,1); // 坐标x
    int32 y = luaL_checkinteger(L,2); // 坐标y

    // 传进来的参数是否为像素坐标
    if ( 0 != lua_toboolean( L,3 ) )
    {
        x = PIX_TO_GRID( x );
        y = PIX_TO_GRID( y );
    }

    lua_pushinteger( L,grid_map::get_pass_cost( x,y ) );

    return 1;
}
