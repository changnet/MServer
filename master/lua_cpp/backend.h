#ifndef __BACKEND_H__
#define __BACKEND_H__

/* 后台工作类 */

#include <lua.hpp>

#include "../global/global.h"
#include "../ev/ev.h"
#include "../net/socket.h"

class backend
{
public:
    explicit backend();
    explicit backend( ev_loop *loop,lua_State *L );
    ~backend();

    int32 run ();
    int32 quit();
    int32 now ();
    
    int32 listen();
    int32 connect();
    int32 send();
    int32 raw_send();
    
    int32 io_kill();
    int32 timer_kill();
    
    int32 set_net_ref();
    int32 fd_address();
    
    void listen_cb( ev_io &w,int revents );
    void read_cb( ev_io &w,int revents );
    void connect_cb( ev_io &w,int32 revents );
    
private:
    void packet_parse( int32 fd,class socket *_socket );

private:
    typedef struct
    {
        ev_timer *w;
        int32 ref;
    }ANTIMER;

    typedef class socket *ANIO;/* AN = array node */

    class ev_loop *loop;
    lua_State *L;
    
    ANIO *anios;
    int32 aniomax;

    ANTIMER *timerlist;
    int32 timerlistmax;
    int32 timerlistcnt;
    
    /* lua层网络回调 */
    int32 net_accept;
    int32 net_read;
    int32 net_disconnect;
    int32 net_connected;
    int32 net_self;
};

#endif /* __BACKEND_H__ */
