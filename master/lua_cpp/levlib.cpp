#include "lenv.h"

#include "../ev/ev.h"

int luaopen_ev (lua_State *L)
{
    lclass<ev_loop> lc(L);
    lc.def<&ev_loop::run>("run");
    lc.def<&ev_loop::done>("done");
    lc.def<&ev_loop::ev_now>("now");
}
