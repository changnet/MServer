#pragma once

#include <lua.hpp>
#include "../global/global.h"
#include "../ev/ev_watcher.h"

class LTimer
{
public:
    explicit LTimer(lua_State *L);
    ~LTimer();

    int32_t set(lua_State *L);
    int32_t stop(lua_State *L);
    int32_t start(lua_State *L);
    int32_t active(lua_State *L);

    void callback(EVTimer &w, int32_t revents);

private:
    int32_t _timer_id;
    class EVTimer _timer;
};
