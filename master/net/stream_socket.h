#ifndef __STREAM_SOCKET_H__
#define __STREAM_SOCKET_H__

#include "socket.h"

class stream_socket : public socket
{
public:
    ~stream_socket();
    explicit stream_socket( uint32 conn_id,conn_t conn_ty );

    void message_cb ( int32 revents );
    void connect_cb ( int32 revents );
    void listen_cb  ( int32 revents );
};

#endif /* __STREAM_SOCKET_H__ */