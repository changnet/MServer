#include <websocket_parser.h>

#include "lpp/ltools.hpp"
#include "system/static_global.hpp"
#include "net/socket.hpp"
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
    // 防止被攻击，游戏中不应该需要这么大的数据包，暂时不考虑其他应用
    if (parser->length > 10 * 1024 * 1024)
    {
        ws_packet->set_error(2);
        ELOG("websocket to large packet :" FMT64u, (uint64_t)parser->length);
        return -1;
    }

    class Buffer &body = ws_packet->body_buffer();

    // 清空数据
    body.clear();

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
        Buffer::Transaction &&ts = body.flat_reserve(length);

        websocket_parser_decode(ts.ctx_, at, length, parser);
        body.commit(ts, (int32_t)length);
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
    if (unlikely(parser->flags & 0x08))
    {
        return ws_packet->on_ctrl_end();
    }
    return ws_packet->on_frame_end();
}

//< init all field insted of using websocket_parser_settings_init
static const struct websocket_parser_settings settings = {
    on_frame_header,
    on_frame_body,
    on_frame_end,
};

///////////////////////////////// WEBSOCKET PARSER /////////////////////////////

WebsocketPacket::WebsocketPacket(class Socket *sk) : HttpPacket(sk)
{
    e_          = 0;
    is_upgrade_ = false;

    parser_ = new struct websocket_parser();
    websocket_parser_init(parser_);
    parser_->data = this;
}

WebsocketPacket::~WebsocketPacket()
{
    is_upgrade_ = false;

    delete parser_;
    parser_ = nullptr;
}

int32_t WebsocketPacket::pack_raw(lua_State *L, int32_t index)
{
    // 允许握手未完成就发数据，自己保证顺序
    // if ( !is_upgrade_ ) return http_packet::pack_clt( L,index );

    websocket_flags flags =
        static_cast<websocket_flags>(luaL_checkinteger(L, index));

    size_t size     = 0;
    const char *ctx = luaL_optlstring(L, index + 1, nullptr, &size);
    // if ( !ctx ) return 0; // 允许发送空包

    size_t len = websocket_calc_frame_size(flags, size);

    Buffer &buffer           = socket_->get_send_buffer();
    Buffer::Transaction &&ts = buffer.flat_reserve(len);

    char mask[4] = {0}; /* 服务器发往客户端并不需要mask */
    if (flags & WS_HAS_MASK) new_masking_key(mask);
    websocket_build_frame(ts.ctx_, flags, mask, ctx, size);

    buffer.commit(ts, (int32_t)len);
    socket_->flush();

    return 0;
}

int32_t WebsocketPacket::pack_clt(lua_State *L, int32_t index)
{
    return pack_raw(L, index);
}

int32_t WebsocketPacket::pack_srv(lua_State *L, int32_t index)
{
    return pack_raw(L, index);
}

int32_t WebsocketPacket::pack_ctrl(lua_State *L, int32_t index)
{
    /* https://tools.ietf.org/html/rfc6455#section-5.5
     * 控制帧可以包含数据。但这个数据不是data-frame，即不能设置OP_TEXT、OP_BINARY
     * 标识的应用数据。这个数据是用来说明当前控制帧的。比如close帧后面包含status
     * code，及 关闭原因，pong数据包则必须原封不动返回ping数据包中的数据
     */
    return pack_raw(L, index);
}

int32_t WebsocketPacket::unpack(Buffer &buffer)
{
    /* 未握手时，由http处理
     * 握手成功后，http中止处理，未处理的数据仍在buffer中，由websocket继续处理
     */
    if (!is_upgrade_) return HttpPacket::unpack(buffer);

    e_        = 0; // 重置上一次解析错误
    bool next = false;

    // 不要用 buffer.all_to_flat_ctx(size); 这个会把收到的数据都拷贝到缓冲区
    // 效率很低。
    do
    {
        size_t size     = 0;
        const char *ctx = buffer.get_front_used(size, next);
        if (size == 0) return 0;

        // websocket_parser_execute把数据全当二进制处理，没有错误返回
        // 解析过程中，如果settings中回调返回非0值，则中断解析并返回已解析的字符数
        size_t nparser = websocket_parser_execute(parser_, &settings, ctx, size);

        buffer.remove(nparser);
        if (nparser != size)
        {
            // websocket_parser未提供错误机制，所有函数都是返回非0值表示中止解析
            // 但中止解析并不表示解析出错，比如
            // 上层脚本在一个消息回调中关闭了连接，则需要中止解析
            // 返回-1表示解析出错，会在底层直接删除链接
            // 中止解析则由上层脚本逻辑决定如何处理(比如关闭链接或者直接忽略)
            return e_ ? -1 : 0;
        }
    } while (next);

    return 0;
}

int32_t WebsocketPacket::on_message_complete(bool upgrade)
{
    // 正在情况下，对方应该只下发一个带upgrade标记的http头来进行握手
    // 但如果对方不是websocket，则可能按http下发404之类的其他东西
    if (!upgrade || is_upgrade_)
    {
        set_error(3);
        ELOG("upgrade error, %s", http_info_.body_.c_str());
        return -1;
    }

    is_upgrade_ = true;
    if (0 != invoke_handshake())
    {
        set_error(4);
        return -1;
    }

    return 0;
}

int32_t WebsocketPacket::invoke_handshake()
{
    /* https://tools.ietf.org/pdf/rfc6455.pdf Section 1.3,page 6
     */

    const char *key_str    = nullptr;
    const char *accept_str = nullptr;

    /* 不知道当前是服务端还是客户端，两个key都查找，由上层处理 */
    const head_map_t &head_field       = http_info_.head_field_;
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

    if (nullptr == key_str && nullptr == accept_str)
    {
        set_error(5);
        ELOG("websocket handshake header field not found");
        return -1;
    }

    lua_State *L = StaticGlobal::L;
    assert(0 == lua_gettop(L));

    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, "handshake_new");
    lua_pushinteger(L, socket_->conn_id());
    lua_pushstring(L, key_str);
    lua_pushstring(L, accept_str);

    if (unlikely(LUA_OK != lua_pcall(L, 3, 0, 1)))
    {
        ELOG("websocket handshake:%s", lua_tostring(L, -1));
    }

    lua_settop(L, 0); /* remove traceback */

    return socket_->is_closed() ? -1 : 0;
}

// 普通websokcet数据帧完成，ctx直接就是字符串，不用decode
int32_t WebsocketPacket::on_frame_end()
{
    lua_State *L = StaticGlobal::L;
    assert(0 == lua_gettop(L));

    size_t size     = 0;
    const char *ctx = body_.all_to_flat_ctx(size);

    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, "command_new");
    lua_pushinteger(L, socket_->conn_id());
    lua_pushlstring(L, ctx, size);

    if (unlikely(LUA_OK != lua_pcall(L, 2, 0, 1)))
    {
        ELOG("websocket command:%s", lua_tostring(L, -1));
    }

    lua_settop(L, 0); /* remove traceback */

    return socket_->is_closed() ? -1 : 0;
}

// 处理ping、pong等opcode
int32_t WebsocketPacket::on_ctrl_end()
{
    lua_State *L = StaticGlobal::L;
    assert(0 == lua_gettop(L));

    size_t size     = 0;
    const char *ctx = body_.all_to_flat_ctx(size);

    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, "ctrl_new");
    lua_pushinteger(L, socket_->conn_id());
    lua_pushinteger(L, parser_->flags);

    int32_t args_cnt = 2;
    if (size > 0)
    {
        args_cnt++;
        // 控制帧也是可以包含数据的
        lua_pushlstring(L, ctx, size);
    }

    if (unlikely(LUA_OK != lua_pcall(L, args_cnt, 0, 1)))
    {
        ELOG("websocket ctrl:%s", lua_tostring(L, -1));
    }

    lua_settop(L, 0); /* remove traceback */

    return socket_->is_closed() ? -1 : 0;
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
