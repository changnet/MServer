#include <lua.hpp>

#include "io.hpp"
#include "system/static_global.hpp"

IO::IO()
{
    role_type_ = ACCEPTOR;
}

IO::~IO()
{
}

bool IO::init_event(EVIO *w, lua_State *L, int32_t index)
{
    int32_t ev = luaL_checkinteger(L, index);
    StaticGlobal::B->set_watcher_event(w, ev);
    return true;
}

bool IO::uninit_event(EVIO *w, lua_State *L, int32_t index)
{
    bool flush = lua_toboolean(L, index);

    // EV_FLUSH不要和EV_CLOSE同时发送，不然EV_FLUSH会失效，这在另一个线程有特殊处理
    StaticGlobal::B->add_watcher_event(w, flush ? EV_FLUSH : EV_CLOSE);
    return true;
}