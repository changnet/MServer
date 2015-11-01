#ifndef __LTIMER_H__
#define __LTIMER_H__

#include <lua.hpp>
#include "../global/global.h"
#include "../ev/ev_watcher.h"

class ltimer
{
public:
    explicit ltimer( lua_State *L );
    ~ltimer();

    int32 start();
    int32 stop();
    int32 active();

    int32 set_self();
    int32 set_callback();

    void callback( ev_timer &w,int32 revents );
private:
    int32 ref_self;
    int32 ref_callback;

    lua_State *L;
    class ev_timer timer;
};

#endif /* __LTIMER_H__ */
