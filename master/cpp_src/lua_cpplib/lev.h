#ifndef __LEVENTLOOP_H__
#define __LEVENTLOOP_H__

/* 事件循环 lua wrap */

#include <lua.hpp>

#include "../global/global.h"
#include "../ev/ev.h"
#include "../ev/ev_watcher.h"

class socket;
class leventloop : public ev_loop
{
public:
    static leventloop *instance();
    static void uninstance();

    explicit leventloop( lua_State *L );
    ~leventloop();

    int32 exit( lua_State *L );
    int32 time( lua_State *L ); // 帧时间
    int32 backend( lua_State *L );
    int32 real_time( lua_State *L ); // 实时时间

    int32 signal( lua_State *L );

    int32 pending_send( class socket *s );
    void remove_pending( int32 pending );
private:
    explicit leventloop( bool singleton );

    void running();
    void invoke_signal ();
    void invoke_sending();

    ev_tstamp wait_time()
    {
        return ansendingcnt > 0 ? backend_mintime : ev_loop::wait_time();
    }
    static void sig_handler( int32 signum );
private:
    typedef class socket *ANSENDING;
    /* 待发送队列 */
    ANSENDING *ansendings;
    int32 ansendingmax;
    int32 ansendingcnt;

    static uint32 sig_mask;
    static class leventloop *_loop;
};

#endif /* __LEVENTLOOP_H__ */
