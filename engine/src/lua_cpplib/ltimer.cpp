#include "ltimer.hpp"
#include "../system/static_global.hpp"
#include "ltools.hpp"

LTimer::LTimer(lua_State *L)
{
    _timer_id = luaL_checkinteger(L, 2);

    _timer.set(StaticGlobal::ev());
    _timer.bind(&LTimer::callback, this);
}

LTimer::~LTimer() {}

int32_t LTimer::set(lua_State *L)
{
    int64_t after  = static_cast<int64_t>(luaL_checkinteger(L, 1));
    int64_t repeat = static_cast<int64_t>(luaL_optinteger(L, 2, 0));

    int32_t policy = luaL_optinteger(L, 3, 0);

    if (after < 0 or repeat < 0)
    {
        return luaL_error(L, "negative timer argument");
    }

    _timer.set(after, repeat);
    _timer.set_policy(policy);

    return 0;
}

int32_t LTimer::start(lua_State *L)
{
    /* 理论上可以多次start而不影响，但会有性能上的消耗 */
    if (_timer.active())
    {
        return luaL_error(L, "timer already start");
    }

    if (_timer.at() <= 0 && _timer.repeat() <= 0)
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
    lua_pushboolean(L, _timer.active());

    return 1;
}

void LTimer::callback(int32_t revents)
{
    assert(!(EV_ERROR & revents));

    static lua_State *L = StaticGlobal::state();

    LUA_PUSHTRACEBACK(L);
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
