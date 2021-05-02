#include "lmap.hpp"
#include "ltools.hpp"
#include "../scene/scene_include.hpp"

LMap::~LMap() {}

LMap::LMap(lua_State *L) {}

int32_t LMap::load(lua_State *L) // 加载地图数据
{
    // TODO: 地图数据格式定下来才能做
    return 0;
}

int32_t LMap::set(lua_State *L) // 设置地图信息(用于动态创建地图)
{
    int32_t id     = luaL_checkinteger32(L, 1);
    int32_t width  = luaL_checkinteger32(L, 2);
    int32_t height = luaL_checkinteger32(L, 3);

    if (width < 0 || height < 0) return 0;

    bool ok = GridMap::set(id, width, height);

    lua_pushboolean(L, ok);
    return 1;
}

int32_t LMap::fill(lua_State *L) // (用于动态创建地图)
{
    int32_t x    = luaL_checkinteger32(L, 1); // 填充的坐标x
    int32_t y    = luaL_checkinteger32(L, 2); // 填充的坐标y
    int32_t cost = luaL_checkinteger32(L, 3); // 该格子的消耗

    assert(cost >= -255 && cost <= 255);
    bool ok = GridMap::fill(x, y, static_cast<int8_t>(cost));

    lua_pushboolean(L, ok);
    return 1;
}

int32_t LMap::get_size(lua_State *L) // 获取地图宽高
{
    int32_t width  = GridMap::get_width();
    int32_t height = GridMap::get_height();

    lua_pushinteger(L, width);
    lua_pushinteger(L, height);

    return 2;
}

int32_t LMap::fork(lua_State *L) // 复制一份地图(用于动态修改地图数据)
{
    // TODO:以后可能会加上区域属性，一些属性需要动态修改
    // 这时候我们可以复制一份地图数据，而不会直接修改基础配置数据

    class LMap **udata = (class LMap **)luaL_checkudata(L, 1, "Map");

    class GridMap *map = *udata;
    if (!map) return 0;

    // TODO:暂时没有对应的数据来做

    return 0;
}

int32_t LMap::get_pass_cost(lua_State *L) // 获取通过某个格子的消耗
{
    int32_t x = (int32_t)luaL_checknumber(L, 1); // 坐标x
    int32_t y = (int32_t)luaL_checknumber(L, 2); // 坐标y

    // 传进来的参数是否为像素坐标
    if (0 != lua_toboolean(L, 3))
    {
        x = PIX_TO_GRID(x);
        y = PIX_TO_GRID(y);
    }

    lua_pushinteger(L, GridMap::get_pass_cost(x, y));

    return 1;
}
