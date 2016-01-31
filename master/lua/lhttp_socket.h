#ifndef __LHTTP_SOCKET_H__
#define __LHTTP_SOCKET_H__

#include <map>
#include <http_parser.h>  /* file at deps/include */
#include "lsocket.h"

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
    explicit lhttp_socket( lua_State *L );
    ~lhttp_socket();

    using lsocket::send;
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
private:
    bool is_message_complete();

private:
    enum parse_state _state;
    http_parser *_parser;
    std::string _url;
    std::string _body;
    std::string _cur_field;
    std::string _old_field;
    std::map< std::string,std::string > _head_field;
};

#endif /* __LHTTP_SOCKET_H__ */
