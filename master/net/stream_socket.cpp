#include "stream_socket.h"

stream_socket::~stream_socket()
{

}

stream_socket::stream_socket( uint32 conn_id,conn_t conn_ty )
    : socket( conn_id,conn_ty )
{
}

void stream_socket::message_cb ( int32 revents )
{

}

void stream_socket::connect_cb ( int32 revents )
{

}

void stream_socket::listen_cb  ( int32 revents )
{

}
