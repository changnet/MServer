#include "lastar.hpp"
#include "lmap.hpp"
#include "ltools.hpp"

int32_t LAstar::search(lua_State *L) // 寻路
{
    class LMap **udata = (class LMap **)luaL_checkudata(L, 1, "Map");

    int32_t x  = luaL_checkinteger32(L, 2); // 起点坐标x
    int32_t y  = luaL_checkinteger32(L, 3); // 起点坐标x
    int32_t dx = luaL_checkinteger32(L, 4); // 终点坐标x
    int32_t dy = luaL_checkinteger32(L, 5); // 终点坐标x

    // 路径放到一个table里，但是C++里不会创建，由脚本那边传入并缓存，防止频繁创建引发gc
    const static int32_t tbl_stack = 6;
    lUAL_CHECKTABLE(L, tbl_stack);

    class GridMap *map = *udata;
    if (!map) return 0;

    if (!AStar::search(map, x, y, dx, dy)) return 0;

    const std::vector<int32_t> &path = AStar::get_path();

    int32_t path_sz = (int32_t)path.size();
    // 路径依次存各个点的x,y坐标，应该是成双的
    if (0 != path_sz % 2) return 0;

    int32_t tbl_idx = 1;
    // 原来的路径是反向的，这里还原
    for (int32_t idx = path_sz - 1; idx > 0; idx -= 2)
    {
        // x坐标
        lua_pushinteger(L, path[idx - 1]);
        lua_rawseti(L, tbl_stack, tbl_idx++);

        // y坐标
        lua_pushinteger(L, path[idx]);
        lua_rawseti(L, tbl_stack, tbl_idx++);
    }

    // 设置table的n值为路径坐标数
    lua_pushstring(L, "n");
    lua_pushinteger(L, path_sz);
    lua_rawset(L, tbl_stack);

    // 返回路径格子数
    lua_pushinteger(L, path_sz / 2);
    return 1;
}
