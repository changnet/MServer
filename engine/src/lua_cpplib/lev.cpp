#include "lev.hpp"
#include "ltools.hpp"

#include "../net/socket.hpp"
#include "../system/static_global.hpp"

LEV::LEV()
{
    _critical_tm               = -1;
    _app_ev._repeat_ms         = 60000;
    _app_ev._next_time         = 0;
}

LEV::~LEV()
{
}

int32_t LEV::exit(lua_State *L)
{
    UNUSED(L);
    EV::quit();
    return 0;
}

int32_t LEV::backend(lua_State *L)
{
    UNUSED(L);
    _done = false;

    loop(); /* this won't return until backend stop */
    return 0;
}

int32_t LEV::time_update(lua_State *L)
{
    UNUSED(L);
    EV::time_update();
    return 0;
}

// 帧时间
int32_t LEV::time(lua_State *L)
{
    lua_pushinteger(L, _rt_time);
    return 1;
}

int32_t LEV::ms_time(lua_State *L) // 帧时间，ms
{
    lua_pushinteger(L, _mn_time);
    return 1;
}

int32_t LEV::who_busy(lua_State *L) // 看下哪条线程繁忙
{
    bool skip = lua_toboolean(L, 1);

    size_t finished   = 0;
    size_t unfinished = 0;
    const char *who =
        StaticGlobal::thread_mgr()->who_is_busy(finished, unfinished, skip);

    if (!who) return 0;

    lua_pushstring(L, who);
    lua_pushinteger(L, finished);
    lua_pushinteger(L, unfinished);

    return 3;
}

// 实时时间
int32_t LEV::real_time(lua_State *L)
{
    lua_pushinteger(L, get_real_time());
    return 1;
}

// 实时时间
int32_t LEV::real_ms_time(lua_State *L)
{
    lua_pushinteger(L, get_monotonic_time());
    return 1;
}

int32_t LEV::signal(lua_State *L)
{
    int32_t sig    = luaL_checkinteger32(L, 1);
    int32_t action = luaL_optinteger32(L, 2, -1);
    if (sig < 1 || sig > 31)
    {
        return luaL_error(L, "illegal signal id:%d", sig);
    }

    Thread::signal(sig, action);

    return 0;
}

int32_t LEV::set_app_ev(lua_State *L) // 设置脚本主循环回调
{
    // 主循环不要设置太长的循环时间，如果太长用定时器就好了
    int32_t interval = luaL_checkinteger32(L, 1);
    if (interval < 0 || interval > 1000)
    {
        return luaL_error(L, "illegal argument");
    }

    _app_ev._repeat_ms = interval;
    _app_ev._next_time = _mn_time;
    return 0;
}

int32_t LEV::set_critical_time(lua_State *L) // 设置主循环临界时间
{
    _critical_tm = luaL_checkinteger32(L, 1);

    return 0;
}

void LEV::invoke_signal()
{
    static lua_State *L = StaticGlobal::state();
    LUA_PUSHTRACEBACK(L);

    int signum       = 0;
    int32_t top      = lua_gettop(L);
    int32_t sig_mask = Thread::signal_mask_once();
    while (sig_mask != 0)
    {
        if (sig_mask & 1)
        {
            lua_getglobal(L, "sig_handler");
            lua_pushinteger(L, signum);
            if (EXPECT_FALSE(LUA_OK != lua_pcall(L, 1, 0, top)))
            {
                ELOG("signal call lua fail:%s", lua_tostring(L, -1));
                lua_pop(L, 1); /* pop error message */
            }
        }
        ++signum;
        sig_mask = (sig_mask >> 1);
    }

    lua_remove(L, top); /* remove traceback */
}

bool LEV::next_periodic(Periodic &periodic)
{
    bool timeout = false;
    int64_t tm   = periodic._next_time - _mn_time;
    if (tm <= 0)
    {
        timeout = true;
        // TODO 当主循环卡了，这两个表现是不一样的，后续有需要再改
        // periodic._next_time += periodic._repeat_ms;
        periodic._next_time = _mn_time + periodic._repeat_ms;
    }

    // set_backend_time_coarse(periodic._next_time);

    return timeout;
}

void LEV::invoke_app_ev()
{
    static lua_State *L = StaticGlobal::state();

    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, "application_ev");
    lua_pushinteger(L, _mn_time);
    if (EXPECT_FALSE(LUA_OK != lua_pcall(L, 1, 0, 1)))
    {
        ELOG("invoke_app_ev fail:%s", lua_tostring(L, -1));
        lua_pop(L, 1); /* pop error message */
    }

    lua_pop(L, 1); /* remove traceback */
}

void LEV::running()
{
    // 如果主循环被阻塞太久，打印日志
    if (_busy_time > _critical_tm)
    {
        PLOG("ev busy: " FMT64d "msec", _busy_time);
    }

    invoke_signal();
    if (next_periodic(_app_ev)) invoke_app_ev();

    StaticGlobal::thread_mgr()->main_routine();

    StaticGlobal::network_mgr()->invoke_delete();
}

int32_t LEV::periodic_start(lua_State *L)
{
    int32_t id     = luaL_checkinteger32(L, 1);
    int64_t after  = luaL_checkinteger(L, 2);
    int64_t repeat = luaL_checkinteger(L, 3);
    int32_t policy = luaL_checkinteger32(L, 4);

    int32_t e = EV::periodic_start(id, after, repeat, policy);
    lua_pushinteger(L, e);

    return 1;
}

int32_t LEV::periodic_stop(lua_State *L)
{
    int32_t id = luaL_checkinteger32(L, 1);

    int32_t e = EV::periodic_stop(id);
    lua_pushinteger(L, e);

    return 1;
}

int32_t LEV::timer_start(lua_State *L)
{
    int32_t id     = luaL_checkinteger32(L, 1);
    int64_t after  = luaL_checkinteger(L, 2);
    int64_t repeat = luaL_checkinteger(L, 3);
    int32_t policy = luaL_checkinteger32(L, 4);

    int32_t e = EV::timer_start(id, after, repeat, policy);
    lua_pushinteger(L, e);

    return 1;
}

int32_t LEV::timer_stop(lua_State *L)
{
    int32_t id = luaL_checkinteger32(L, 1);

    int32_t e = EV::timer_stop(id);
    lua_pushinteger(L, e);

    return 1;
}

void LEV::timer_callback(int32_t id, int32_t revents)
{
    assert(!(EV_ERROR & revents));

    static lua_State *L = StaticGlobal::state();

    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, "timer_event");
    lua_pushinteger(L, id);

    if (EXPECT_FALSE(LUA_OK != lua_pcall(L, 1, 0, 1)))
    {
        ELOG("timer call back fail:%s\n", lua_tostring(L, -1));
        lua_pop(L, 2); /* remove error message and traceback function */
        return;
    }
    lua_pop(L, 1); /* remove traceback */
}