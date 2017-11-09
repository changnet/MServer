#include "web_socket.h"

#include <websocket_parser.h>

int on_frame_header( struct websocket_parser *parser )
{
    return 0;
}

int on_frame_body( 
    struct websocket_parser *parser, const char * at, size_t length )
{
    return 0;
}

int on_frame_end( struct websocket_parser *parser )
{
    return 0;
}

static const struct websocket_parser_settings settings = 
{
    on_frame_header,
    on_frame_body,
    on_frame_end,
};

web_socket::~web_socket()
{
    delete _parser;
    _parser = NULL;
}

web_socket::web_socket( uint32 conn_id,conn_t conn_ty )
    :socket( conn_id,conn_ty )
{
    _parser = new struct websocket_parser();
    websocket_parser_init( _parser );
    _parser->data = this;
}


void web_socket::command_cb()
{

}

void web_socket::connect_cb()
{

}

void web_socket::listen_cb ()
{

}
