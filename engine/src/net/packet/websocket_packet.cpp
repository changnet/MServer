#include <websocket_parser.h>

#include "../../lua_cpplib/ltools.hpp"
#include "../../system/static_global.hpp"
#include "../socket.hpp"
#include "websocket_packet.hpp"

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
[   8 ]bit :
用于标识PayloadData是否经过掩码处理。如果是1，Masking-key域的数据即是
                掩码密钥，用于解码PayloadData。客户端发出的数据帧需要进行掩码处理，
                所以此位是1。服务端发出的数据帧不能设置此标志位。
[ 9,15]bit : Payload data的长度,7bit
    如果其值在0-125，则是payload的真实长度。
    如果值是126，则后面2个字节形成的16bits无符号整型数的值是payload的真实长度。注意，网络字节序，需要转换。
    如果值是127，则后面8个字节形成的64bits无符号整型数的值是payload的真实长度。注意，网络字节序，需要转换。

!!! 下面的字段可能不存在，需要根据Payload len的值偏移，这里按最大计算。
[16,31]bit : Payload len = 126,这16bit构成一个uint16类型表示Payload Data的长度
[16,79]bit : Payload len = 126,这16bit构成一个uint64类型表示Payload Data的长度
[80,111]bit:
上面的第8bit值为1时，这里的32bit表示Masking-key。客户端发给服务器的包
                必须有masking-key。The masking key is a 32-bit value chosen at
                random ,needs to be unpredictable
                原因见：https://tools.ietf.org/pdf/rfc6455.pdf section 5.3
page33
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

// 解析完websocket的header
int32_t on_frame_header(struct websocket_parser *parser)
{
    assert(parser && parser->data);

    class WebsocketPacket *ws_packet =
        static_cast<class WebsocketPacket *>(parser->data);
    class Buffer &body = ws_packet->body_buffer();

    // parser->data->opcode = parser->flags & WS_OP_MASK; // gets opcode
    // parser->data->is_final = parser->flags & WS_FIN;   // checks is final
    // frame websocket是允许不发内容的，因此length可能为0
    body.clear();
    if (parser->length)
    {
        if (!body.reserved(parser->length))
        {
            ELOG("websocket cant not allocate memory");
            return -1;
        }
    }
    return 0;
}

// 收到帧数据
int32_t on_frame_body(struct websocket_parser *parser, const char *at,
                      size_t length)
{
    assert(parser && parser->data);

    class WebsocketPacket *ws_packet =
        static_cast<class WebsocketPacket *>(parser->data);
    class Buffer &body = ws_packet->body_buffer();

    // 如果带masking-key，则收到的body都需要用masking-key来解码才能得到原始数据
    if (parser->flags & WS_HAS_MASK)
    {
        // if ( !body.reserved( length ) ) return -1;
        // 不再reserved，在frame_header里应该已reserved的。而且，正常情况下，websocket
        // 应该只用到单个接收缓冲区。如果单个放不下最大协议，考虑修改缓冲区大小。目前缓冲区没
        // 法reserved超过一个chunk大小的连续缓冲区
        if (body.get_space_size() < length)
        {
            ELOG("websocket packet on frame body overflow:%d,%d",
                  body.get_used_size(), body.get_space_size());
            return -1;
        }

        websocket_parser_decode(body.get_space_ctx(), at, length, parser);
        body.add_used_offset(length);
    }
    else
    {
        body.append(at, length);
    }
    return 0;
}

// 数据帧完成
int32_t on_frame_end(struct websocket_parser *parser)
{
    assert(parser && parser->data);

    class WebsocketPacket *ws_packet =
        static_cast<class WebsocketPacket *>(parser->data);

    /* https://tools.ietf.org/html/rfc6455#section-5.5
     * opcode并不是按位来判断的，而是按顺序1、2、3、4...
     * 它们是互斥的，只能存在其中一个,我们只需要判断最高位即可(Control frames are
     * identified by opcodes where the most significant bit of the opcode is 1)
     */
    if (EXPECT_FALSE(parser->flags & 0x08))
    {
        return ws_packet->on_ctrl_end();
    }
    return ws_packet->on_frame_end();
}

/* init all field insted of using websocket_parser_settings_init */
static const struct websocket_parser_settings settings = {
    on_frame_header,
    on_frame_body,
    on_frame_end,
};

///////////////////////////////// WEBSOCKET PARSER /////////////////////////////

WebsocketPacket::WebsocketPacket(class Socket *sk) : HttpPacket(sk)
{
    _is_upgrade = false;

    _parser = new struct websocket_parser();
    websocket_parser_init(_parser);
    _parser->data = this;
}

WebsocketPacket::~WebsocketPacket()
{
    _is_upgrade = false;

    delete _parser;
    _parser = NULL;
}

int32_t WebsocketPacket::pack_raw(lua_State *L, int32_t index)
{
    // 允许握手未完成就发数据，自己保证顺序
    // if ( !_is_upgrade ) return http_packet::pack_clt( L,index );

    websocket_flags flags =
        static_cast<websocket_flags>(luaL_checkinteger(L, index));

    size_t size     = 0;
    const char *ctx = luaL_optlstring(L, index + 1, NULL, &size);
    // if ( !ctx ) return 0; // 允许发送空包

    size_t len         = websocket_calc_frame_size(flags, size);
    class Buffer &send = _socket->send_buffer();
    if (!send.reserved(len))
    {
        return luaL_error(L, "can not reserved buffer");
    }

    char mask[4] = {0}; /* 服务器发往客户端并不需要mask */
    if (flags & WS_HAS_MASK) new_masking_key(mask);
    websocket_build_frame(send.get_space_ctx(), flags, mask, ctx, size);
    send.add_used_offset(len);
    _socket->pending_send();

    return 0;
}

/* 打包服务器发往客户端数据包
 * return: <0 error;>=0 success
 */
int32_t WebsocketPacket::pack_clt(lua_State *L, int32_t index)
{
    return pack_raw(L, index);
}

/* 打包客户端发往服务器数据包
 * return: <0 error;>=0 success
 */
int32_t WebsocketPacket::pack_srv(lua_State *L, int32_t index)
{
    return pack_raw(L, index);
}

// 发送opcode
// 对应websocket，可以直接用pack_clt或pack_srv发送控制帧。这个函数是给ws_stream等子类使用
int32_t WebsocketPacket::pack_ctrl(lua_State *L, int32_t index)
{
    /* https://tools.ietf.org/html/rfc6455#section-5.5
     * 控制帧可以包含数据。但这个数据不是data-frame，即不能设置OP_TEXT、OP_BINARY
     * 标识的应用数据。这个数据是用来说明当前控制帧的。比如close帧后面包含status
     * code，及 关闭原因，pong数据包则必须原封不动返回ping数据包中的数据
     */
    return pack_raw(L, index);
}

/* 数据解包
 * return: <0 error;0 success
 */
int32_t WebsocketPacket::unpack()
{
    /* 未握手时，由http处理
     * 握手成功后，http中止处理，未处理的数据仍在buffer中，由websocket继续处理
     */
    if (!_is_upgrade) return HttpPacket::unpack();

    class Buffer &recv = _socket->recv_buffer();

    size_t size   = 0;
    const char *ctx = recv.all_to_continuous_ctx(size);
    if (size == 0) return 0;

    // websocket_parser_execute把数据全当二进制处理，没有错误返回
    // 解析过程中，如果settings中回调返回非0值，则中断解析并返回已解析的字符数
    size_t nparser = websocket_parser_execute(_parser, &settings, ctx, size);
    // 如果未解析完，则是严重错误，比如分配不到内存。而websocket_parser只回调一次结果，
    // 因为不能返回0。返回0造成循环解析，但内存不一定有分配
    // 普通错误，比如回调脚本出错，是不会中止解析的
    if (nparser != size)
    {
        _socket->stop();
        return -1;
    }

    recv.remove(nparser);

    return 0;
}

/* http-parser在解析完握手数据时，会触发一次message_complete */
int32_t WebsocketPacket::on_message_complete(bool upgrade)
{
    assert(upgrade && !_is_upgrade);

    _is_upgrade = true;
    invoke_handshake();

    return 0;
}

int32_t WebsocketPacket::invoke_handshake()
{
    /* https://tools.ietf.org/pdf/rfc6455.pdf Section 1.3,page 6
     */

    const char *key_str    = NULL;
    const char *accept_str = NULL;

    /* 不知道当前是服务端还是客户端，两个key都查找，由上层处理 */
    const head_map_t &head_field       = _http_info._head_field;
    head_map_t::const_iterator key_itr = head_field.find("Sec-WebSocket-Key");
    if (key_itr != head_field.end())
    {
        key_str = key_itr->second.c_str();
    }
    else
    {
        head_map_t::const_iterator accept_itr =
            head_field.find("Sec-WebSocket-Accept");
        if (accept_itr != head_field.end())
        {
            accept_str = accept_itr->second.c_str();
        }
    }

    if (NULL == key_str && NULL == accept_str)
    {
        ELOG("websocket handshake header field not found");
        return -1;
    }

    static lua_State *L = StaticGlobal::state();
    assert(0 == lua_gettop(L));

    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, "handshake_new");
    lua_pushinteger(L, _socket->conn_id());
    lua_pushstring(L, key_str);
    lua_pushstring(L, accept_str);

    if (EXPECT_FALSE(LUA_OK != lua_pcall(L, 3, 0, 1)))
    {
        ELOG("websocket handshake:%s", lua_tostring(L, -1));
    }

    lua_settop(L, 0); /* remove traceback */

    return _socket->fd() < 0 ? -1 : 0;
}

// 普通websokcet数据帧完成，ctx直接就是字符串，不用decode
int32_t WebsocketPacket::on_frame_end()
{
    static lua_State *L = StaticGlobal::state();
    assert(0 == lua_gettop(L));

    size_t size   = 0;
    const char *ctx = _body.all_to_continuous_ctx(size);

    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, "command_new");
    lua_pushinteger(L, _socket->conn_id());
    lua_pushlstring(L, ctx, size);

    if (EXPECT_FALSE(LUA_OK != lua_pcall(L, 2, 0, 1)))
    {
        ELOG("websocket command:%s", lua_tostring(L, -1));
    }

    lua_settop(L, 0); /* remove traceback */

    return _socket->fd() < 0 ? -1 : 0;
}

// 处理ping、pong等opcode
int32_t WebsocketPacket::on_ctrl_end()
{
    static lua_State *L = StaticGlobal::state();
    assert(0 == lua_gettop(L));

    size_t size   = 0;
    const char *ctx = _body.all_to_continuous_ctx(size);

    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, "ctrl_new");
    lua_pushinteger(L, _socket->conn_id());
    lua_pushinteger(L, _parser->flags);
    // 控制帧也是可以包含数据的
    lua_pushlstring(L, ctx, size);

    if (EXPECT_FALSE(LUA_OK != lua_pcall(L, 3, 0, 1)))
    {
        ELOG("websocket ctrl:%s", lua_tostring(L, -1));
    }

    lua_settop(L, 0); /* remove traceback */

    return _socket->fd() < 0 ? -1 : 0;
}

void WebsocketPacket::new_masking_key(char mask[4])
{
    /* George Marsaglia  Xorshift generator
     * www.jstatsoft.org/v08/i14/paper
     */
    static unsigned long x = 123456789, y = 362436069, z = 521288629;

    // period 2^96-1
    unsigned long t;
    x ^= x << 16;
    x ^= x >> 5;
    x ^= x << 1;

    t = x;
    x = y;
    y = z;
    z = t ^ x ^ y;

    uint32_t *new_mask = reinterpret_cast<uint32_t *>(mask);
    *new_mask          = static_cast<uint32_t>(z);
}
