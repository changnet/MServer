#pragma once

#include <lua.hpp>
#include "../scene/a_star.h"

class lastar : public a_star
{
public:
    ~lastar() {};
    explicit lastar( lua_State *L ) {};

    int32 search( lua_State *L ); // 寻路
};
