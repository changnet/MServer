#ifndef __LSOCKET_H__
#define __LSOCKET_H__

#include <lua.hpp>
#include "../net/socket.h"
#include "../ev/ev_def.h"
#include "../ev/ev_watcher.h"

class lsocket : public socket
{
public:
    explicit lsocket( lua_State *L );
    ~lsocket();

    int32 send();
    int32 kill();
    int32 address ();
    int32 listen  ();
    int32 connect ();
    int32 raw_send();
    
    int32 set_self();
    int32 set_read();
    int32 set_accept();
    int32 set_connected();
    int32 set_disconnected();
    
    void read_cb   ( ev_io &w,int32 revents );
    void listen_cb ( ev_io &w,int32 revents );
    void connect_cb( ev_io &w,int32 revents );
private:
    void on_disconnect();
    void packet_parse();
private:
    int32 ref_self;
    int32 ref_read;
    int32 ref_accept;
    int32 ref_disconnect;
    int32 ref_connected;

    lua_State *L;
};

#endif /* __LSOCKET_H__ */
