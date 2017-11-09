#ifndef __WEB_SOCKET_H__
#define __WEB_SOCKET_H__

#include "socket.h"

struct websocket_parser;
class web_socket : public socket
{
public:
    ~web_socket();
    explicit web_socket( uint32 conn_id,conn_t conn_ty );

    void command_cb ();
    void connect_cb ();
    void listen_cb  ();

private:
    struct websocket_parser *_parser;
};

#endif /* __WEB_SOCKET_H__ */