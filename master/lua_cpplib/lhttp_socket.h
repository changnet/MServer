#ifndef __LHTTP_SOCKET_H__
#define __LHTTP_SOCKET_H__

#include <map>
#include <queue>
#include "lsocket.h"
#include "lclass.h"

struct http_parser;

class lhttp_socket : public lsocket
{
public:
    struct http_info
    {
        uint32 _method;
        uint32 _status_code;
        std::string _url;
        std::string _body;
        std::map< std::string,std::string > _head_field;
    };
public:
    ~lhttp_socket();
    explicit lhttp_socket( lua_State *L );

    int32 next();
    int32 get_url();
    int32 get_body();
    int32 is_upgrade();
    int32 get_method();
    int32 get_status();
    int32 get_head_field();

    void on_message_complete();
    void listen_cb( int32 revents );

    void append_url( const char *at,size_t len );
    void append_body( const char *at,size_t len );
    void append_cur_field( const char *at,size_t len );
    void append_cur_value( const char *at,size_t len );

    /* 以下函数因为lua粘合层的写法限制，需要在子类覆盖，不然无法注册 */
    inline int32 send   () { return lsocket::send(); }
    inline int32 kill   () { return lsocket::kill(); }
    inline int32 address() { return lsocket::address(); }
    inline int32 listen () { return lsocket::listen (); }
    inline int32 connect() { return lsocket::connect(); }

    inline int32 set_self_ref  () { return lsocket::set_self_ref  (); }
    inline int32 set_on_command() { return lsocket::set_on_command(); }
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
        lclass<lhttp_socket>::push( L,this,true );
    }
private:
    bool _upgrade;
    http_parser *_parser;
    std::string _cur_field;
    std::string _old_field;
    struct http_info *_cur_http;
    std::queue<struct http_info *> _http;
};

#endif /* __LHTTP_SOCKET_H__ */
