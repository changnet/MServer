#ifndef __LSOCKET_H__
#define __LSOCKET_H__

#include <lua.hpp>
#include "../net/socket.h"

class lsocket : public socket
{
public:
    virtual ~lsocket();

    void message_cb ( int32 revents );
    void connect_cb ( int32 revents );
    void listen_cb  ( int32 revents );
protected:
    explicit lsocket( lua_State *L );

    int32 send();
    int32 kill();
    int32 address ();
    int32 listen  ();
    int32 connect ();

    int32 set_self_ref  ();
    int32 set_on_message();
    int32 buffer_setting();
    int32 set_on_acception ();
    int32 set_on_connection();
    int32 set_on_disconnect();
    int32 file_description ();

    virtual void push() const = 0;
    virtual int32 is_message_complete() = 0;
    virtual const class lsocket *accept_new( int32 fd ) = 0;
private:
    void on_disconnect();
protected:
    int32 ref_self;
    int32 ref_message;
    int32 ref_acception;
    int32 ref_disconnect;
    int32 ref_connection;

    lua_State *L;
};

#endif /* __LSOCKET_H__ */
