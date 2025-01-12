#include "ev.hpp"
#include "lpp/llib.hpp"
#include "lpp/lcpp.hpp"
#include "system/static_global.hpp"
#include "time.hpp"

EV::EV()
{
    stop_              = true;
    L_                 = nullptr;
}

EV::~EV()
{
    assert(!L_);
}

void EV::routinue()
{
    static const int64_t min_wait = 1;  // 最小等待时间，毫秒

    while (likely(!stop_))
    {
        timing::update();

        int64_t wait_time = timer_mgr_.next_interval();
        if (unlikely(wait_time < min_wait)) wait_time = min_wait;

        // 等待其他线程的数据
        wait_for(wait_time);

        timing::update();

        timer_mgr_.update_timeout(this);

        dispatch_message();
    }
}

void EV::dispatch_message()
{
    while (true)
    {
        try
        {
            ThreadMessage m = pop_message();
            if (-1 == m.src_) return;

            lcpp::call(L_, "main_message_dispatch", m.src_, m.dst_, m.type_,
                       m.udata_, m.usize_);
            m.dispose();
        }
        catch (const std::runtime_error &e)
        {
            ELOG("%s", e.what());
        }
    }
}

bool EV::init_entry(int32_t argc, char **argv)
{
    lua_pushcfunction(L_, traceback);
    /* 加载程序入口脚本 */
    if (LUA_OK != luaL_loadfile(L_, LUA_ENTERANCE))
    {
        const char *err_msg = lua_tostring(L_, -1);
        ELOG_R("load lua enterance file error:%s", err_msg);

        lua_pop(L_, 2); // pop error message and traceback
        return false;
    }

    lua_checkstack(L_, argc);

    /* push argv to lua */
    for (int i = 0; i < argc; i++)
    {
        lua_pushstring(L_, argv[i]);
    }

    if (LUA_OK != lua_pcall(L_, argc, 0, 1))
    {
        const char *err_msg = lua_tostring(L_, -1);
        ELOG("call lua enterance file error:%s", err_msg);

        lua_pop(L_, 2); // pop error message and traceback
        return false;
    }
    lua_pop(L_, 1); // traceback

    return true;
}

void EV::start(int32_t argc, char **argv)
{
    L_ = llib::new_state();

    timing::update();
    if (!init_entry(argc, argv))
    {
        L_ = llib::delete_state(L_);
        return;
    }

    stop_ = false;
    routinue();
    stop_ = true;

    L_    = llib::delete_state(L_);
}

void EV::stop()
{
    stop_ = true;
}
