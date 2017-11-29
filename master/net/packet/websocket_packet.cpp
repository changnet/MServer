#include <websocket_parser.h>

#include "../socket.h"
#include "websocket_packet.h"
#include "../../lua_cpplib/ltools.h"
#include "../../lua_cpplib/lstate.h"

/*
https://tools.ietf.org/pdf/rfc6455.pdf sector 5.2 page28
0 1 2 3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-------+-+-------------+-------------------------------+
|F|R|R|R| opcode|M| Payload len | Extended payload length |
|I|S|S|S| (4) |A| (7) | (16/64) |
|N|V|V|V| |S| | (if payload len==126/127) |
| |1|2|3| |K| | |
+-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
| Extended payload length continued, if payload len == 127 |
+ - - - - - - - - - - - - - - - +-------------------------------+
| |Masking-key, if MASK set to 1 |
+-------------------------------+-------------------------------+
| Masking-key (continued) | Payload Data |
+-------------------------------- - - - - - - - - - - - - - - - +
: Payload Data continued ... :
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
| Payload Data continued ... |
+---------------------------------------------------------------+

*/

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

websocket_packet::websocket_packet( class socket *sk ) : http_packet( sk )
{
    _is_upgrade = false;

    _parser = new struct websocket_parser();
    websocket_parser_init( _parser );
    _parser->data = this;
}

websocket_packet::~websocket_packet()
{
    _is_upgrade = false;

    delete _parser;
    _parser = NULL;
}

/* 打包服务器发往客户端数据包
 * return: <0 error;>=0 success
 */
int32 websocket_packet::pack_clt( lua_State *L,int32 index )
{
    if ( !_is_upgrade ) return http_packet::pack_clt( L,index );

    return 0;
}

/* 打包客户端发往服务器数据包
 * return: <0 error;>=0 success
 */
int32 websocket_packet::pack_srv( lua_State *L,int32 index )
{
    if ( !_is_upgrade ) return http_packet::pack_srv( L,index );

    return 0;
}

/* 数据解包 
 * return: <0 error;0 success
 */
int32 websocket_packet::unpack()
{
    /* 未握手时，由http处理
     * 握手成功后，http中止处理，未处理的数据仍在buffer中，由websocket继续处理
     */
    if ( !_is_upgrade ) return http_packet::unpack();

    return 0;
}

/* 由http升级为websocket，发握手数据 */
int32 websocket_packet::upgrade()
{
    _is_upgrade = true;

    return 0;
}

int32 websocket_packet::invoke_handshake()
{
    /* https://tools.ietf.org/pdf/rfc6455.pdf Section 1.3,page 6
     */

    const char *key_str = NULL;
    const char *accept_str = NULL;

    /* 不知道当前是服务端还是客户端，两个key都查找，由上层处理 */
    const head_map_t &head_field = _http_info._head_field;
    head_map_t::const_iterator key_itr = head_field.find( "Sec-WebSocket-Key" );
    if ( key_itr != head_field.end() )
    {
        key_str = key_itr->second.c_str();
    }

    head_map_t::const_iterator accept_itr = 
        head_field.find( "Sec-WebSocket-Accept" );
    if ( accept_itr != head_field.end() )
    {
        accept_str = accept_itr->second.c_str();
    }

    if ( NULL == key_str && NULL == accept_str )
    {
        ERROR( "websocket handshake header field not found" );
        return -1;
    }

    static lua_State *L = lstate::instance()->state();
    assert( "lua stack dirty",0 == lua_gettop(L) );

    lua_pushcfunction( L,traceback );
    lua_getglobal    ( L,"ws_handshake_new" );
    lua_pushinteger  ( L,_socket->conn_id() );
    lua_pushstring   ( L,key_str );
    lua_pushstring   ( L,accept_str );

    if ( expect_false( LUA_OK != lua_pcall( L,3,0,1 ) ) )
    {
        ERROR( "websocket handshake:%s",lua_tostring( L,-1 ) );
    }

    lua_settop( L,0 ); /* remove traceback */

    return _socket->fd() < 0 ? -1 : 0;
}
