#include "lev.hpp"
#include "ltools.hpp"

#include "net/socket.hpp"
#include "system/static_global.hpp"

LEV::LEV()
{
    critical_tm_       = -1;
    app_repeat_        = 60000;
    app_next_tm_       = 0;
}

LEV::~LEV()
{
}

int32_t LEV::backend(lua_State *L)
{
    UNUSED(L);
    done_ = false;

    // 这个函数不再返回，当前堆栈上有一个无用的ev对象，要清掉
    // 否则其他函数就要特殊处理这个堆栈
    lua_settop(L, 0);
    loop();
    return 0;
}

int64_t LEV::time()
{
    return system_now_;
}

int32_t LEV::who_busy(lua_State *L) // 看下哪条线程繁忙
{
    bool skip = lua_toboolean(L, 2);

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

int32_t LEV::signal(int32_t sig, int32_t action)
{
    if (sig < 1 || sig > 31)
    {
        return luaL_error(StaticGlobal::L, "illegal signal id:%d", sig);
    }

    //Thread::signal(sig, action);

    return 0;
}

void LEV::set_app_ev(int32_t interval)
{
    // 主循环不要设置太长的循环时间，如果太长用定时器就好了
    if (interval < 0 || interval > 60000)
    {
        luaL_error(StaticGlobal::L, "illegal argument");
        return;
    }

    app_repeat_  = interval;
    app_next_tm_ = steady_clock_;
    return;
}

int32_t LEV::set_critical_time(lua_State *L) // 设置主循环临界时间
{
    critical_tm_ = luaL_checkinteger32(L, 2);

    return 0;
}

void LEV::invoke_signal()
{
    lua_State *L = StaticGlobal::L;
    LUA_PUSHTRACEBACK(L);

    int signum       = 0;
    int32_t top      = lua_gettop(L);
    int32_t sig_mask = 0; // Thread::signal_mask_once();
    while (sig_mask != 0)
    {
        if (sig_mask & 1)
        {
            lua_getglobal(L, "sig_handler");
            lua_pushinteger(L, signum);
            if (unlikely(LUA_OK != lua_pcall(L, 1, 0, top)))
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

void LEV::invoke_app_ev()
{
    if (steady_clock_ < app_next_tm_)
    {
        next_backend_time_ = app_next_tm_;
        return;
    }
    app_next_tm_ = steady_clock_ + app_repeat_;

    lua_State *L = StaticGlobal::L;

    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, "application_ev");
    lua_pushinteger(L, steady_clock_);
    if (unlikely(LUA_OK != lua_pcall(L, 1, 0, 1)))
    {
        ELOG("invoke_app_ev fail:%s", lua_tostring(L, -1));
        lua_pop(L, 1); /* pop error message */
    }

    lua_pop(L, 1); /* remove traceback */
}

void LEV::running()
{
    // 如果主循环被阻塞太久，打印日志
    if (busy_time_ > critical_tm_)
    {
        PLOG("ev busy: " FMT64d "msec", busy_time_);
    }

    invoke_signal();
    invoke_app_ev();

    StaticGlobal::thread_mgr()->main_routine();
}

int32_t LEV::periodic_start(lua_State *L)
{
    int32_t id     = luaL_checkinteger32(L, 2);
    int64_t after  = luaL_checkinteger(L, 3);
    int64_t repeat = luaL_checkinteger(L, 4);
    int32_t policy = luaL_checkinteger32(L, 5);

    int32_t e = EV::periodic_start(id, after, repeat, policy);
    lua_pushinteger(L, e);

    return 1;
}

int32_t LEV::periodic_stop(lua_State *L)
{
    int32_t id = luaL_checkinteger32(L, 2);

    int32_t e = EV::periodic_stop(id);
    lua_pushinteger(L, e);

    return 1;
}

int32_t LEV::timer_start(lua_State *L)
{
    int32_t id     = luaL_checkinteger32(L, 2);
    int64_t after  = luaL_checkinteger(L, 3);
    int64_t repeat = luaL_checkinteger(L, 4);
    int32_t policy = luaL_checkinteger32(L, 5);

    int32_t e = EV::timer_start(id, after, repeat, policy);
    lua_pushinteger(L, e);

    return 1;
}

int32_t LEV::timer_stop(int32_t id)
{
    return EV::timer_stop(id);
}

void LEV::timer_callback(int32_t id, int32_t revents)
{
    assert(!(EV_ERROR & revents));

    lua_State *L = StaticGlobal::L;

    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, "timer_event");
    lua_pushinteger(L, id);

    if (unlikely(LUA_OK != lua_pcall(L, 1, 0, 1)))
    {
        ELOG("timer call back fail:%s\n", lua_tostring(L, -1));
        lua_pop(L, 2); /* remove error message and traceback function */
        return;
    }
    lua_pop(L, 1); /* remove traceback */
}
