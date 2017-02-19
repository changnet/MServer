#ifndef __LSTREAM_SOCKET_H__
#define __LSTREAM_SOCKET_H__

#include "lsocket.h"
#include "lclass.h"

class lstream_socket :public lsocket
{
public:
    ~lstream_socket();
    explicit lstream_socket( lua_State *L );

    int32 srv_next(); /* get next server packet */
    int32 clt_next(); /* get next client packet */
    int32 css_cmd (); /* get client to server to server cmd */

    int32 rpc_send  (); /* send rpc packet */
    int32 rpc_decode(); /* decode rpc packet */
    int32 rpc_call( int32 index,int32 oldtop,int32 rpc_res );

    int32 ss_flatbuffers_send   (); /* server to server */
    int32 ss_flatbuffers_decode (); /* decode server to server packet */
    int32 sc_flatbuffers_send   (); /* server to client */
    int32 ssc_flatbuffers_send  (); /* server to server to client */
    int32 cs_flatbuffers_decode (); /* decode client to server packet */
    int32 cs_flatbuffers_send   (); /* client to server */
    int32 css_flatbuffers_send  (); /* client to server to server */
    int32 css_flatbuffers_decode(); /* decode client to server to server packet*/
public:
    void listen_cb  ( int32 revents );
    /* 以下函数因为lclass的写法限制，需要在子类覆盖，不然无法注册 */
    inline int32 send   () { return lsocket::send(); }
    inline int32 kill   () { return lsocket::kill(); }
    inline int32 address() { return lsocket::address(); }
    inline int32 listen () { return lsocket::listen (); }
    inline int32 connect() { return lsocket::connect(); }

    inline int32 set_self_ref  () { return lsocket::set_self_ref  (); }
    inline int32 set_on_message() { return lsocket::set_on_message(); }
    inline int32 buffer_setting() { return lsocket::buffer_setting(); }
    inline int32 set_on_acception () { return lsocket::set_on_acception (); }
    inline int32 set_on_connection() { return lsocket::set_on_connection(); }
    inline int32 set_on_disconnect() { return lsocket::set_on_disconnect(); }
    inline int32 file_description () { return lsocket::file_description (); }
private:
    int32 is_message_complete();

    const class lsocket *accept_new( int32 fd );

    void push() const
    {
        lclass<lstream_socket>::push( L,this,true );
    }
};

#endif /* __LSTREAM_SOCKET_H__ */
