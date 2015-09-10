#ifndef __BACKEND_H__
#define __BACKEND_H__

/* 后台工作类 */

#include <lua.hpp>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>  /* htons */

#include "../global/global.h"
#include "../ev/ev.h"
#include "../ev/ev_watcher.h"

class backend
{
public:
    explicit backend();

    int32 run ();
    int32 quit();
    int32 now ();
    int32 listen();

    void set( ev_loop *loop,lua_State *L );
    
    static inline int32 noblock( int32 fd )
    {
        int32 flags = fcntl( fd, F_GETFL, 0 ); //get old status
        if ( flags == -1 )
            return -1;

        flags |= O_NONBLOCK;

        return fcntl( fd, F_SETFL, flags);
    }

private:
    class ev_loop *loop;
    lua_State *L;
    
    typedef ev_io *pio;
    typedef ev_timer *ptimer;

    pio *iolist;
    int32 iolistmax;
    int32 iolistcnt;

    ptimer *timerlist;
    int32 timerlistmax;
    int32 timerlistcnt;
};

#endif /* __BACKEND_H__ */
