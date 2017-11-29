#include <websocket_parser.h>

#include "websocket_packet.h"


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

///////////////////////////////// WEBSOCKET PARSER /////////////////////////////

websocket_packet::websocket_packet( class socket *sk ) : packet( sk )
{
    _parser = new struct websocket_parser();
    websocket_parser_init( _parser );
    _parser->data = this;
}

websocket_packet::~websocket_packet()
{
    delete _parser;
    _parser = NULL;
}

/* 打包服务器发往客户端数据包
 * return: <0 error;>=0 success
 */
int32 websocket_packet::pack_clt( lua_State *L,int32 index )
{
    return 0;
}

/* 打包客户端发往服务器数据包
 * return: <0 error;>=0 success
 */
int32 websocket_packet::pack_srv( lua_State *L,int32 index )
{
    return 0;
}

/* 数据解包 
 * return: <0 error;0 success
 */
int32 websocket_packet::unpack()
{
    return 0;
}
