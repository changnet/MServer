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

    int set();
    int32 stop  ();
    int32 start ();
    int32 active();

    void callback( ev_timer &w,int32 revents );
private:
    int32 _timer_id;

    lua_State *L;
    class ev_timer _timer;
};

#endif /* __LTIMER_H__ */
