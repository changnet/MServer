#ifndef __STREAM_SOCKET_H__
#define __STREAM_SOCKET_H__

#include "socket.h"

class stream_socket : public socket
{
public:
    ~stream_socket();
    explicit stream_socket( uint32 conn_id,conn_t conn_ty );

    void command_cb ();
    void connect_cb ();
    void listen_cb  ();
private:
    void process_packet();
};

#endif /* __STREAM_SOCKET_H__ */