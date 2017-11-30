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
                必须有masking-key。The masking key is a 32-bit value chosen at 
                random ,needs to be unpredictable
                原因见：https://tools.ietf.org/pdf/rfc6455.pdf section 5.3 page33
[...] 具体的数据

有masking key时，数据必须用masking key编码、解码，算法如下(section 5.3)：
   The masking does not affect the length of the "Payload data".  To
   convert masked data into unmasked data, or vice versa, the following
   algorithm is applied.  The same algorithm applies regardless of the
   direction of the translation, e.g., the same steps are applied to
   mask the data as to unmask the data.

   Octet i of the transformed data ("transformed-octet-i") is the XOR of
   octet i of the original data ("original-octet-i") with octet at index
   i modulo 4 of the masking key ("masking-key-octet-j"):

     j                   = i MOD 4
     transformed-octet-i = original-octet-i XOR masking-key-octet-j

   The payload length, indicated in the framing as frame-payload-length,
   does NOT include the length of the masking key.  It is the length of
   the "Payload data", e.g., the number of bytes following the masking
   key.
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

/* init all field insted of using websocket_parser_settings_init */
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

    size_t nread = websocket_parser_execute( _parser, &settings, data, data_len);
    if(nread != data_len) {
        // some callback return a value other than 0
    }

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
