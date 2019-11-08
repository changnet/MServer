#include "ltimer.h"
#include "ltools.h"
#include "../system/static_global.h"

LTimer::LTimer(lua_State *L)
{
    _timer_id = luaL_checkinteger(L, 2);

    _timer.set(StaticGlobal::ev());
    _timer.set<LTimer, &LTimer::callback>(this);
}

LTimer::~LTimer() {}

int32_t LTimer::set(lua_State *L)
{
    EvTstamp after  = static_cast<EvTstamp>(luaL_checknumber(L, 1));
    EvTstamp repeat = static_cast<EvTstamp>(luaL_optnumber(L, 2, 0.));

    bool time_jump = false;
    if (lua_isboolean(L, 3)) time_jump = lua_toboolean(L, 3);

    if (after < 0. or repeat < 0.)
    {
        return luaL_error(L, "negative timer argument");
    }

    _timer.set(after, repeat);
    _timer.set_time_jump(time_jump);

    return 0;
}

int32_t LTimer::start(lua_State *L)
{
    /* 理论上可以多次start而不影响，但会有性能上的消耗 */
    if (_timer.is_active())
    {
        return luaL_error(L, "timer already start");
    }

    if (_timer.at <= 0. && _timer.repeat <= 0.)
    {
        return luaL_error(L, "timer no repeater interval set");
    }

    _timer.start();

    return 0;
}

int32_t LTimer::stop(lua_State *L)
{
    _timer.stop();

    return 0;
}

int32_t LTimer::active(lua_State *L)
{
    lua_pushboolean(L, _timer.is_active());

    return 1;
}

void LTimer::callback(EVTimer &w, int32_t revents)
{
    ASSERT(!(EV_ERROR & revents), "libev timer cb error");

    static lua_State *L = StaticGlobal::state();

    lua_pushcfunction(L, traceback);
    lua_getglobal(L, "timer_event");
    lua_pushinteger(L, _timer_id);

    if (EXPECT_FALSE(LUA_OK != lua_pcall(L, 1, 0, 1)))
    {
        ERROR("timer call back fail:%s\n", lua_tostring(L, -1));
        lua_pop(L, 2); /* remove error message traceback */
        return;
    }
    lua_pop(L, 1); /* remove traceback */
}
