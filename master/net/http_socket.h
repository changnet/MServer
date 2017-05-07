#ifndef __HTTP_SOCKET_H__
#define __HTTP_SOCKET_H__

#include <map>
#include <queue>

#include "socket.h"

struct http_parser;
class http_socket : public socket
{
public:
    struct http_info
    {
        std::string _url;
        std::string _body;
        std::map< std::string,std::string > _head_field;
    };
public:
    ~http_socket();
    explicit http_socket( uint32 conn_id,conn_t conn_ty );

    void command_cb ();
    void connect_cb ();
    void listen_cb  ();

    bool upgrade() const;
    uint32 status() const;
    const char *method() const;
    const struct http_info &get_http() const;
public:
    /* http_parse 回调函数 */
    void reset_parse();
    void on_headers_complete();
    void on_message_complete();
    void append_url( const char *at,size_t len );
    void append_body( const char *at,size_t len );
    void append_cur_field( const char *at,size_t len );
    void append_cur_value( const char *at,size_t len );
private:
    http_parser *_parser;
    std::string _cur_field;
    std::string _cur_value;
    struct http_info _http_info;
};

#endif /* __HTTP_SOCKET_H__ */