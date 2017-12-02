#include <websocket_parser.h>

#include "../socket.h"
#include "websocket_packet.h"
#include "../../lua_cpplib/ltools.h"
#include "../../lua_cpplib/lstate.h"

/*
https://tools.ietf.org/pdf/rfc6455.pdf sector 5.2 page28

 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-------+-+-------------+-------------------------------+
|F|R|R|R| opcode|M| Payload len | Extended payload length       |
|I|S|S|S|   (4) |A|     (7)     |          (16/64)              |
|N|V|V|V|       |S|             | (if payload len==126/127)     |
| |1|2|3|       |K|             |                               |
+-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
| Extended payload length continued, if payload len == 127      |
+ - - - - - - - - - - - - - - - +-------------------------------+
|                               |Masking-key, if MASK set to 1  |
+-------------------------------+-------------------------------+
| Masking-key (continued)       | Payload Data                  |
+-------------------------------- - - - - - - - - - - - - - - - +
: Payload Data continued ...                                    :
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
| Payload Data continued ...                                    |
+---------------------------------------------------------------+

[   0 ]bit : 标识是否为此消息的最后一个数据包
[ 1,3 ]bit : 用于扩展协议，一般为0
[ 4,7 ]bit : opcode，4bit，数据包类型（frame type）
                0x0：标识一个中间数据包
                0x1：标识一个text类型数据包
                0x2：标识一个binary类型数据包
                0x3-7：保留
                0x8：标识一个断开连接类型数据包
                0x9：标识一个ping类型数据包
                0xA：表示一个pong类型数据包
                0xB-F：保留
[   8 ]bit : 用于标识PayloadData是否经过掩码处理。如果是1，Masking-key域的数据即是
                掩码密钥，用于解码PayloadData。客户端发出的数据帧需要进行掩码处理，
                所以此位是1。服务端发出的数据帧不能设置此标志位。
[ 9,15]bit : Payload data的长度,7bit
    如果其值在0-125，则是payload的真实长度。
    如果值是126，则后面2个字节形成的16bits无符号整型数的值是payload的真实长度。注意，网络字节序，需要转换。
    如果值是127，则后面8个字节形成的64bits无符号整型数的值是payload的真实长度。注意，网络字节序，需要转换。

!!! 下面的字段可能不存在，需要根据Payload len的值偏移，这里按最大计算。
[16,31]bit : Payload len = 126,这16bit构成一个uint16类型表示Payload Data的长度
[16,79]bit : Payload len = 126,这16bit构成一个uint64类型表示Payload Data的长度
[80,111]bit: 上面的第8bit值为1时，这里的32bit表示Masking-key。客户端发给服务器的包
                必须有masking-key。
                原因见：https://tools.ietf.org/html/rfc6455#section-10.3
[...] 具体的数据
*/

// 解析完websocket的header
int32 on_frame_header( struct websocket_parser *parser )
{
    assert( "websocket parser NULL",parser && parser->data );

    class websocket_packet *ws_packet =
        static_cast<class websocket_packet *>( parser->data );
    class buffer &body = ws_packet->body_buffer();

    // parser->data->opcode = parser->flags & WS_OP_MASK; // gets opcode
    // parser->data->is_final = parser->flags & WS_FIN;   // checks is final frame
    // websocket是允许不发内容的，因此length可能为0
    body.clear();
    if( parser->length )
    {
        if ( !body.reserved( parser->length ) )
        {
            ERROR( "websocket cant not allocate memory" );
            return -1;
        }
    }
    return 0;
}

// 收到帧数据
int32 on_frame_body( 
    struct websocket_parser *parser, const char * at, size_t length )
{
    assert( "websocket parser NULL",parser && parser->data );

    class websocket_packet *ws_packet =
        static_cast<class websocket_packet *>( parser->data );
    class buffer &body = ws_packet->body_buffer();

    // 如果带masking-key，则收到的body都需要用masking-key来解码才能得到原始数据
    if( parser->flags & WS_HAS_MASK ) {
        if ( !body.reserved( length ) ) return -1;

        websocket_parser_decode( body._buff + body._size, at, length, parser);
        body._size += length;
    }
    else
    {
        if ( !body.append( at,length ) ) return -1;
    }
    return 0;
}

// 数据帧完成
int32 on_frame_end( struct websocket_parser *parser )
{
    assert( "websocket parser NULL",parser && parser->data );

    class websocket_packet *ws_packet =
        static_cast<class websocket_packet *>( parser->data );

    return ws_packet->on_frame_end();
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

    size_t size = 0;
    const char *ctx = luaL_checklstring( L,index,&size );
    if ( !ctx ) return 0;

    class buffer &send = _socket->send_buffer();
    if ( !send.reserved( size ) )
    {
        return luaL_error( L,"can not reserved buffer" );
    }

    websocket_flags flags = 
        static_cast<websocket_flags>( WS_OP_TEXT | WS_FINAL_FRAME );
    size_t len = websocket_calc_frame_size( flags,size );

    char mask[4] = { '1','3','4','6' };
    websocket_build_frame( send._buff + send._size,flags,mask,ctx,size );
    send._size += len;
    _socket->pending_send();

    return 0;
}

/* 打包客户端发往服务器数据包
 * return: <0 error;>=0 success
 */
int32 websocket_packet::pack_srv( lua_State *L,int32 index )
{
    if ( !_is_upgrade ) return http_packet::pack_srv( L,index );

    size_t size = 0;
    const char *ctx = luaL_checklstring( L,index,&size );
    if ( !ctx ) return 0;

    class buffer &send = _socket->send_buffer();
    if ( !send.reserved( size ) )
    {
        return luaL_error( L,"can not reserved buffer" );
    }

    websocket_flags flags = 
    static_cast<websocket_flags>( WS_OP_TEXT | WS_FINAL_FRAME | WS_HAS_MASK );
    size_t len = websocket_calc_frame_size( flags,size );

    char mask[4] = { '1','3','4','6' };
    websocket_build_frame( send._buff + send._size,flags,mask,ctx,size );
    send._size += len;
    _socket->pending_send();

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

    class buffer &recv = _socket->recv_buffer();
    uint32 size = recv.data_size();
    if ( size == 0 ) return 0;

    // websocket_parser_execute把数据全当二进制处理，没有错误返回
    // 解析过程中，如果settings中回调返回非0值，则中断解析并返回已解析的字符数
    size_t nparser = 
        websocket_parser_execute( _parser,&settings,recv.data(),size );
    // 如果未解析完，则是严重错误，比如分配不到内存。而websocket_parser只回调一次结果，
    // 因为不能返回0。返回0造成循环解析，但内存不一定有分配
    // 普通错误，比如回调脚本出错，是不会中止解析的
    if ( nparser != size )
    {
        _socket->stop();
        return -1;
    }

    recv.subtract( nparser );

    return 0;
}

/* http-parser在解析完握手数据时，会触发一次message_complete */
int32 websocket_packet::on_message_complete( bool upgrade )
{
    assert( "should be upgrade",upgrade );
    // 先触发脚本握手才标识为websocket，因为握手进还需要发http
    _is_upgrade = true;

    invoke_handshake();

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

int32 websocket_packet::on_frame_end()
{
    static lua_State *L = lstate::instance()->state();
    assert( "lua stack dirty",0 == lua_gettop(L) );

    lua_pushcfunction( L,traceback );
    lua_getglobal    ( L,"webs_command_new" );
    lua_pushinteger  ( L,_socket->conn_id() );
    lua_pushlstring   ( L,_body.data(),_body.data_size() );

    if ( expect_false( LUA_OK != lua_pcall( L,2,0,1 ) ) )
    {
        ERROR( "websocket command:%s",lua_tostring( L,-1 ) );
    }

    lua_settop( L,0 ); /* remove traceback */

    return _socket->fd() < 0 ? -1 : 0;
}
