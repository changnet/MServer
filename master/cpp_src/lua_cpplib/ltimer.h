#pragma once

#include <lua.hpp>
#include "../global/global.h"
#include "../ev/ev_watcher.h"

class ltimer
{
public:
    explicit ltimer( lua_State *L );
    ~ltimer();

    int32 set   ( lua_State *L );
    int32 stop  ( lua_State *L );
    int32 start ( lua_State *L );
    int32 active( lua_State *L );

    void callback( ev_timer &w,int32 revents );
private:
    int32 _timer_id;
    class ev_timer _timer;
};
