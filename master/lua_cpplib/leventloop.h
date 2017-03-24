#ifndef __LEVENTLOOP_H__
#define __LEVENTLOOP_H__

/* 事件循环 lua wrap */

#include <lua.hpp>

#include "../global/global.h"
#include "../ev/ev.h"
#include "../ev/ev_watcher.h"
#include "../net/socket.h"

class leventloop : public ev_loop
{
public:
    static leventloop *instance();
    static void uninstance();

    explicit leventloop( lua_State *L );
    ~leventloop();

    int32 exit();
    int32 time();
    int32 backend();

    int32 signal();
    int32 set_signal_ref();

    void finalize();
    int32 pending_send( class socket *s );
    void remove_sending( int32 sending );
private:
    explicit leventloop( lua_State *L,bool singleton );

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

    lua_State *L;
    int32 sig_ref;
    static uint32 sig_mask;
    static class leventloop *_loop;
};

#endif /* __LEVENTLOOP_H__ */
