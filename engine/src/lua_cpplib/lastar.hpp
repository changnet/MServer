#pragma once

#include "scene/a_star.hpp"
#include <lua.hpp>

/**
 * A*寻路
 */
class LAstar : public AStar
{
public:
    ~LAstar() {}
    explicit LAstar(lua_State *L) {}

    /**
     * A*寻路
     * @param x 起点格子x坐标
     * @param y 起点格子y坐标
     * @param dx 终点格子x坐标
     * @param dy 终点格子y坐标
     * @param tbl 存储路径的Lua table，长度存放在字段n中
     * @return 返回路径格子数
     */
    int32_t search(lua_State *L); // 寻路
};
