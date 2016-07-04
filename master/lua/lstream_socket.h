#ifndef __LSTREAM_SOCKET_H__
#define __LSTREAM_SOCKET_H__

#include "lsocket.h"

class lstream_socket :public lsocket
{
public:
    ~lstream_socket();
    explicit lstream_socket( lua_State *L );

    int32 s2c_send();
    int32 s2s_send();
    int32 c2s_send();
    int32 unpack_client();

    int32 pack_server();
    int32 unpack_server();
public:
    void listen_cb  ( int32 revents );
    /* 以下函数因为lua粘合层的写法限制，需要在子类覆盖，不然无法注册 */
    inline int32 send   () { return lsocket::send(); }
    inline int32 kill   () { return lsocket::kill(); }
    inline int32 address() { return lsocket::address(); }
    inline int32 listen () { return lsocket::listen (); }
    inline int32 connect() { return lsocket::connect(); }

    inline int32 set_self_ref  () { return lsocket::set_self_ref  (); }
    inline int32 set_on_message() { return lsocket::set_on_message(); }
    inline int32 set_on_acception () { return lsocket::set_on_acception (); }
    inline int32 set_on_connection() { return lsocket::set_on_connection(); }
    inline int32 set_on_disconnect() { return lsocket::set_on_disconnect(); }
    inline int32 file_description () { return lsocket::file_description (); }
private:
    int32 is_message_complete();
};

#endif /* __LSTREAM_SOCKET_H__ */
