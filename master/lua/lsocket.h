#ifndef __LSOCKET_H__
#define __LSOCKET_H__

#include <lua.hpp>
#include "../net/socket.h"
#include "../ev/ev_def.h"
#include "../ev/ev_watcher.h"

class lsocket : public socket
{
protected:
    explicit lsocket( lua_State *L );
public:
    virtual ~lsocket();

    int32 send();
    int32 kill();
    int32 address ();
    int32 listen  ();
    int32 connect ();

    int32 set_self_ref();
    int32 set_on_message();
    int32 set_on_acception();
    int32 set_on_connection();
    int32 set_on_disconnect();

    int32 file_description();

    void message_cb ( ev_io &w,int32 revents );
    void connect_cb ( ev_io &w,int32 revents );

    virtual bool is_message_complete() = 0;
    virtual void listen_cb  ( ev_io &w,int32 revents ) = 0;
private:
    void on_disconnect();
    void message_notify();
protected:
    int32 ref_self;
    int32 ref_message;
    int32 ref_acception;
    int32 ref_disconnect;
    int32 ref_connection;

    lua_State *L;
};

#endif /* __LSOCKET_H__ */
