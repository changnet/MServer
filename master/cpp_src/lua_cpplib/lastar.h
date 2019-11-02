#pragma once

#include <lua.hpp>
#include "../scene/a_star.h"

class LAstar : public AStar
{
public:
    ~LAstar() {}
    explicit LAstar( lua_State *L ) {}

    int32_t search( lua_State *L ); // 寻路
};
