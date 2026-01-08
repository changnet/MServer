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
            ThreadMessage *m = pop_message();
            if (!m) return;

            cb_message_ = m;
            void *buffer =
                (m->mask_ & FlexiblePool::BUFFER) ? m->buffer() : nullptr;
            lcpp::call(L_, "main_message_dispatch", m->src_, m->dst_, m->type_,
                       buffer, m->usize_);
            // 如果这个消息被转到其他线程或者复用，则cb_message_为nullptr
            if (cb_message_ == m)
            {
                cb_message_ = nullptr;
                dispose_message(m);
            }
        }
        catch (const std::runtime_error &e)
        {
            ELOG("%s", e.what());
        }
    }
}

bool EV::init_entry(const char *path, int32_t argc, char **argv)
{
    lua_pushcfunction(L_, traceback);

    /* 加载程序入口脚本 */
    if (LUA_OK != luaL_loadfile(L_, path))
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
        // 入口文件加载失败，ELOG未初始化，不能使用
        ELOG_R("call lua enterance file error:%s", err_msg);

        lua_pop(L_, 2); // pop error message and traceback
        return false;
    }
    lua_pop(L_, 1); // traceback

    return true;
}

void EV::start(const char *path, int32_t argc, char **argv)
{
    L_ = llib::new_state();

    timing::update();
    if (!init_entry(path, argc, argv))
    {
        L_ = llib::delete_state(L_);
        return;
    }

    stop_ = false;
    routinue();
    stop_ = true;

    L_ = llib::delete_state(L_);
}

void EV::stop()
{
    stop_ = true;
}

int32_t EV::construct_message(lua_State *L)
{
    int32_t src  = luaL_checkinteger32(L, 2);
    int32_t dst  = luaL_checkinteger32(L, 3);
    uint16_t type  = (uint16_t)luaL_checkinteger(L, 4);
    int32_t usize = luaL_checkinteger32(L, 5);

    // 这里不允许使用usize来传数据，无buffer的直接使用emplace_message
    if (usize <= 0) return luaL_error(L, "invalid size");

    ThreadMessage *m = StaticGlobal::F->construct<ThreadMessage>(
        (size_t)usize, src, dst, type, usize);

    lua_pushlightuserdata(L, (void *)m);
    lua_pushlightuserdata(L, (void *)m->buffer());

    return 2;
}

int32_t EV::destruct_message(lua_State *L)
{
    if (!lua_islightuserdata(L, 2)) return luaL_error(L, "invalid message ptr");

    ThreadMessage *m = (ThreadMessage *)lua_touserdata(L, 2);
    dispose_message(m);

    return 0;
}
int32_t EV::unpack_message(lua_State* L)
{
    if (!lua_islightuserdata(L, 2)) return luaL_error(L, "invalid message ptr");

    ThreadMessage *m = (ThreadMessage *)lua_touserdata(L, 2);

    lua_pushinteger(L, m->src_);
    lua_pushinteger(L, m->dst_);
    lua_pushinteger(L, m->type_);
    lua_pushlightuserdata(L, m->buffer());
    lua_pushinteger(L, m->usize_);
    return 5;
}

int32_t EV::push(lua_State* L, bool gc)
{
    lcpp::Class<EV>::push(L, this, gc);
    return 1;
}
