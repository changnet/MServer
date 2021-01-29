#include "lev.hpp"
#include "ltools.hpp"

#include "../net/socket.hpp"
#include "../system/static_global.hpp"

LEV::LEV()
{
    ansendings   = NULL;
    ansendingmax = 0;
    ansendingcnt = 0;

    _critical_tm       = -1;
    _app_ev._repeat_ms = 60000;
    _thread_routine._repeat_ms = 5;
}

LEV::~LEV()
{
    if (ansendings) delete[] ansendings;
    ansendings   = NULL;
    ansendingcnt = 0;
    ansendingmax = 0;
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
    loop_done = false;

    loop(); /* this won't return until backend stop */
    return 0;
}

int32_t LEV::kernel_info(lua_State *L)
{
#ifdef __linux__
    const char *env = "linux";
#elif __CYGWIN__
    const char *env        = "cygwin";
#else
    const char *env = "unknow";
#endif

#ifdef __MINGW64__
    const char *complier   = "mingw64";
    const char *complier_v = __VERSION__;
#elif __MINGW32__
    const char *complier   = "mingw32";
    const char *complier_v = __VERSION__;
#elif __GNUC__
    // https://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html
    const char *complier   = "gcc";
    const char *complier_v = __VERSION__;
#else
    const char *complier   = "unknow";
    const char *complier_v = "unknow";
#endif

    lua_pushstring(L, env);
    lua_pushstring(L, complier);
    lua_pushstring(L, complier_v);
    lua_pushstring(L, BACKEND_KERNEL);
    lua_pushstring(L, __TIMESTAMP__);

    return 5;
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
    lua_pushinteger(L, ev_rt_now);
    return 1;
}

int32_t LEV::ms_time(lua_State *L) // 帧时间，ms
{
    lua_pushinteger(L, ev_now_ms);
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
    lua_pushinteger(L, get_time());
    return 1;
}

// 实时时间
int32_t LEV::real_ms_time(lua_State *L)
{
    lua_pushinteger(L, get_ms_time());
    return 1;
}

int32_t LEV::signal(lua_State *L)
{
    int32_t sig    = luaL_checkinteger(L, 1);
    int32_t action = luaL_optinteger(L, 2, -1);
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
    int32_t interval = luaL_checkinteger(L, 1);
    if (interval < 0 || interval > 1000)
    {
        return luaL_error(L, "illegal argument");
    }

    _app_ev._repeat_ms = interval;
    _app_ev._next_time = ev_now_ms;
    return 0;
}

int32_t LEV::set_critical_time(lua_State *L) // 设置主循环临界时间
{
    _critical_tm = luaL_checkinteger(L, 1);

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
                ERROR("signal call lua fail:%s", lua_tostring(L, -1));
                lua_pop(L, 1); /* pop error message */
            }
        }
        ++signum;
        sig_mask = (sig_mask >> 1);
    }

    lua_remove(L, top); /* remove traceback */
}

int32_t LEV::pending_send(class Socket *s)
{
    // 0位是空的，不使用
    ++ansendingcnt;
    ARRAY_RESIZE(ANSENDING, ansendings, ansendingmax, ansendingcnt + 1,
                 ARRAY_NOINIT);
    ansendings[ansendingcnt] = s;

    return ansendingcnt;
}

void LEV::remove_pending(int32_t pending)
{
    assert(pending > 0 && pending < ansendingmax);

    ansendings[pending] = NULL;
}

/* 把数据攒到一起，一次发送
 * 好处是：把包整合，减少发送次数，提高效率
 * 坏处是：需要多一个数组管理；如果发送的数据量很大，在逻辑处理过程中就不能利用带宽
 * 然而，游戏中包多，但数据量不大
 */
void LEV::invoke_sending()
{
    if (ansendingcnt <= 0) return;

    int32_t pos       = 0;
    class Socket *skt = NULL;

    /* 0位是空的，不使用 */
    for (int32_t pending = 1; pending <= ansendingcnt; pending++)
    {
        if (!(skt = ansendings[pending])) /* 可能调用了remove_sending */
        {
            continue;
        }

        assert(pending == skt->get_pending());

        /* 处理发送,
         * return: < 0 error,= 0 success,> 0 bytes still need to be send
         */
        if (skt->send() <= 0) continue;

        /* 还有数据，处理sendings数组移动，防止中间留空 */
        if (pending > pos)
        {
            ++pos;
            ansendings[pos] = skt;
            skt->set_pending(pos);
        }
        assert(pos == skt->get_pending());
        /* 数据未发送完，也不需要移动，则do nothing */
    }

    ansendingcnt = pos;
    // 如果还有数据未发送，尽快下发 TODO 这个取值大概是多少合适？
    if (ansendingcnt > 0) set_backend_time_coarse(ev_now_ms + BACKEND_MIN_TM);
    assert(ansendingcnt >= 0 && ansendingcnt < ansendingmax);
}

bool LEV::next_periodic(Periodic &periodic)
{
    bool timeout = false;
    EvTstamp tm = periodic._next_time - ev_now_ms;
    if (tm <= 0)
    {
        timeout = true;
        // TODO 当主循环卡了，这两个表现是不一样的，后续有需要再改
        // periodic._next_time += periodic._repeat_ms;
        periodic._next_time = ev_now_ms + periodic._repeat_ms;
    }

    set_backend_time_coarse(periodic._next_time);

    return timeout;
}

void LEV::invoke_app_ev()
{
    static lua_State *L = StaticGlobal::state();

    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, "application_ev");
    lua_pushinteger(L, ev_now_ms);
    if (EXPECT_FALSE(LUA_OK != lua_pcall(L, 1, 0, 1)))
    {
        ERROR("invoke_app_ev fail:%s", lua_tostring(L, -1));
        lua_pop(L, 1); /* pop error message */
    }

    lua_pop(L, 1); /* remove traceback */
}

void LEV::running()
{
    // 如果主循环被阻塞太久，打印日志
    if (_busy_time > _critical_tm)
    {
        PRINTF("ev busy:%.3f msec", _busy_time);
    }

    invoke_sending();
    invoke_signal();
    if (next_periodic(_app_ev)) invoke_app_ev();
    if (next_periodic(_thread_routine))
    {
        StaticGlobal::thread_mgr()->main_routine();
    }

    StaticGlobal::network_mgr()->invoke_delete();
}
