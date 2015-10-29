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
    
    static int32 callback_ref( lua_State *L );
    
    void read_cb   ( ev_io &w,int32 revents );
    void listen_cb ( ev_io &w,int32 revents );
    void connect_cb( ev_io &w,int32 revents );
private:
    void on_disconnect();
    void packet_parse();
private:
    /* static导致所有socket在lua层只能使用同一个回调，并且只能有一个lua_State
     * 但如果不这样，需要每个socket保存多个ref,将会导致lua注册表增大
     */
    static int32 ref_self;
    static int32 ref_read;
    static int32 ref_accept;
    static int32 ref_disconnect;
    static int32 ref_connected;

    lua_State *L;
};

#endif /* __LSOCKET_H__ */
