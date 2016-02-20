#ifndef __LHTTP_SOCKET_H__
#define __LHTTP_SOCKET_H__

#include <map>
#include "lsocket.h"

struct http_parser;

class lhttp_socket : public lsocket
{
public:
    enum parse_state
    {
        PARSE_NONE  = 0,
        PARSE_PROC  = 1,
        PARSE_DONE  = 2
    };
public:
    ~lhttp_socket();
    explicit lhttp_socket( lua_State *L );

    int32 get_head_field();
    int32 get_url();
    int32 get_body();
    int32 get_method();
    int32 get_status();

    void listen_cb( ev_io &w,int32 revents );

    void set_state( enum parse_state st );
    void append_url( const char *at,size_t len );
    void append_body( const char *at,size_t len );
    void append_cur_field( const char *at,size_t len );
    void append_cur_value( const char *at,size_t len );
    
    /* 以下函数因为lua粘合层的写法限制，需要在子类覆盖，不然无法注册 */
    int32 send   () { return lsocket::send(); }
    int32 kill   () { return lsocket::kill(); }
    int32 address() { return lsocket::address(); }
    int32 listen () { return lsocket::listen (); }
    int32 connect() { return lsocket::connect(); }
    
    int32 set_self_ref  () { return lsocket::set_self_ref  (); }
    int32 set_on_message() { return lsocket::set_on_message(); }
    int32 set_on_acception () { return lsocket::set_on_acception (); }
    int32 set_on_connection() { return lsocket::set_on_connection(); }
    int32 set_on_disconnect() { return lsocket::set_on_disconnect(); }
    int32 file_description () { return lsocket::file_description (); }
private:
    bool is_message_complete();

private:
    enum parse_state _state;
    bool _upgrade;
    http_parser *_parser;
    std::string _url;
    std::string _body;
    std::string _cur_field;
    std::string _old_field;
    std::map< std::string,std::string > _head_field;
};

#endif /* __LHTTP_SOCKET_H__ */
