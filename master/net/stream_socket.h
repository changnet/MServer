#ifndef __STREAM_SOCKET_H__
#define __STREAM_SOCKET_H__

#include "socket.h"
#include "packet.h"

class stream_socket : public socket
{
public:
    ~stream_socket();
    explicit stream_socket( uint32 conn_id,conn_t conn_ty );

    void command_cb ();
    void connect_cb ();
    void listen_cb  ();

    /* 由于需要在C++处理数据包的自动转发，需要标识每个连接属性哪个玩家或服务器
     * owner为玩家id或服务器唯一标识。当连接未认证(比如玩家还未创建角色)owner
     * 为0。未认证的包不应该自动转发，防止被恶意刷数据。
     */
    owner_t get_owner() const { return _owner; }
private:
    void process_packet();
private:
    owner_t _owner;
};

#endif /* __STREAM_SOCKET_H__ */