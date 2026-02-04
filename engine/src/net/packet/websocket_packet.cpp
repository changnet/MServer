#include "lpp/ltools.hpp"
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
static int32_t on_frame_header(struct websocket_parser *parser)
{
    // payload的长度一直保存在parser->length中，这里不需要做任何事

    return 0;
}

// 收到帧数据
static int32_t on_frame_body(struct websocket_parser *parser, const char *at,
                             size_t length)
{
    assert(parser && parser->data);

    class WebsocketPacket *ws_packet =
        static_cast<class WebsocketPacket *>(parser->data);

    // 数据保留在缓冲区中，不需要拷贝出来
    ws_packet->add_payload_len(length);

    return 0;
}

// 数据帧完成
static int32_t on_frame_end(struct websocket_parser *parser)
{
    return WPE_PAUSE; // 终止解析
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
    is_upgrade_ = false;

    to_remove_   = 0;
    payload_len_ = 0;
    parser_      = new struct websocket_parser();
    websocket_parser_init(parser_);
    parser_->data = this;
}

WebsocketPacket::~WebsocketPacket()
{
    is_upgrade_ = false;

    delete parser_;
    parser_ = nullptr;
}

void WebsocketPacket::pack_header(Buffer &buffer, websocket_flags flags,
                                  const char mask[4], const char *data,
                                  size_t data_len)
{
    // websocket的header需要2~14字节，这几个字节没法拆开处理，不能使用append_from_processor
    char header[32];
    size_t offset = websocket_build_frame_header(header, flags, mask, data_len);
    buffer.append(header, offset);
}

void WebsocketPacket::pack_frame(Buffer& buffer, websocket_flags flags,
    const char mask[4],
    const char* data, size_t data_len, uint8_t &mask_offset)
{
    buffer.append_from_processor(
        data, data_len,
        [&mask, flags, &mask_offset](const char *wdata, char *wptr, int64_t space)
        { websocket_append_frame(wptr, flags, mask, wdata, space, &mask_offset); });
}

int32_t WebsocketPacket::pack_any(lua_State *L, int32_t index)
{
    // 允许握手未完成就发数据，自己保证顺序
    // if ( !is_upgrade_ ) return http_packet::pack_clt( L,index );

    websocket_flags flags =
        static_cast<websocket_flags>(luaL_checkinteger(L, index));

    size_t size     = 0;
    const char *ctx = luaL_optlstring(L, index + 1, nullptr, &size);
    // if ( !data ) return 0; // 允许发送空包

    char mask[4] = {0}; /* 服务器发往客户端并不需要mask */
    if (flags & WS_HAS_MASK) new_masking_key(mask);

    Buffer &buffer = socket_->get_send_buffer();

    pack_header(buffer, flags, mask, ctx, size);

    uint8_t mask_offset = 0;
    pack_frame(buffer, flags, mask, ctx, size, mask_offset);

    socket_->flush();

    return 0;
}

int32_t WebsocketPacket::pack_clt(lua_State *L, int32_t index)
{
    return pack_any(L, index);
}

int32_t WebsocketPacket::pack_srv(lua_State *L, int32_t index)
{
    return pack_any(L, index);
}

int32_t WebsocketPacket::pack_ctrl(lua_State *L, int32_t index)
{
    /* https://tools.ietf.org/html/rfc6455#section-5.5
     * 控制帧可以包含数据。但这个数据不是data-frame，即不能设置OP_TEXT、OP_BINARY
     * 标识的应用数据。这个数据是用来说明当前控制帧的。比如close帧后面包含status
     * code，及 关闭原因，pong数据包则必须原封不动返回ping数据包中的数据
     */
    return pack_any(L, index);
}

int64_t WebsocketPacket::buffer_process(bool &term,
    const websocket_parser_settings *settings,
    const char* data, int64_t len)
{
    // websocket_parser_execute把数据全当二进制处理，没有错误返回
    // 解析过程中，如果settings中回调返回非0值，则中断解析并返回已解析的字符数
    size_t nparser = websocket_parser_execute(parser_, settings, data, len);
    assert(0 != nparser);

    int32_t e = parser_->error;
    if (WPE_OK == e)
    {
        // parser_解析完head时不提供任何方法获取head的长度，
        // 只能在第一次解析到payload时手动计算head长度 4 == s_body
        if (4 == parser_->state && parser_->length)
        {
            int64_t head_len = nparser - payload_len_;

            assert(head_len > 0);

            term = true;

            // 这里返回head_len，那buffer那边就会执行删除只保留payload
            // 如果head和payload不在同一块chunk，删除后效率更高
            // 如果在同一块chunk，删除会多一次atomic操作
            return head_len;
        }
        return nparser;
    }
    else if (WPE_PAUSE == e)
    {
        term = true;
        int64_t remove = nparser - parser_->length;
        assert(remove >= 0);
        return remove; // payload还在缓冲区中，只删除head部分
    }

    term = true;
    return nparser;
}

int32_t WebsocketPacket::unpack(lua_State *L, Buffer &buffer)
{
    /* 未握手时，由http处理
     * 握手成功后，http中止处理，未处理的数据仍在buffer中，由websocket继续处理
     */
    if (!is_upgrade_) return HttpPacket::unpack(L, buffer);

    if (to_remove_) // 上一个协议的数据，开始解析下一个协议前先删除旧协议
    {
        buffer.remove_head_data(to_remove_);
        to_remove_ = 0;
    }

    int32_t e       = WPE_OK;
    L_              = L;

    // 如果已经解析出了payload长度，直接对比数据长度就行，不再解析
    if (0 == parser_->length)
    {
        buffer.iterate_to_processor(
            [this, settings = &settings](bool &term, const char *rptr, int64_t size)
            { return buffer_process(term, settings, rptr, size); });
        e = parser_->error;
    }

    if (WPE_OK == e)
    {
        if (0 == parser_->length || buffer.length() < (int64_t)parser_->length)
        {
            return unpack_return(L, PC_MORE);
        }
        e = WPE_PAUSE;
    }
    if (WPE_PAUSE == e)
    {
        /* https://tools.ietf.org/html/rfc6455#section-5.5
         * opcode并不是按位来判断的，而是按顺序1、2、3、4...
         * 它们是互斥的，只能存在其中一个,我们只需要判断最高位即可(Control frames are
         * identified by opcodes where the most significant bit of the opcode is 1)
         *
         */
        int32_t argc = (parser_->flags & 0x08) ? on_ctrl_end() : on_frame_end();

        payload_len_ = 0;
        to_remove_ = parser_->length;

        // 与llhttp不一样，websocket_parser不能用pause停下来，否则state状态不对
        // 类似llhttp_resume，这里重置让它能pause并继续解析
        websocket_parser_init(parser_);

        return argc;
    }

    return unpack_error(L, "websocket parse error");
}

int32_t WebsocketPacket::on_upgrade(lua_State *L)
{
    // 这个是由http_packet回调
    // 正在情况下，对方应该只下发一个带upgrade标记的http头来进行握手
    // 但如果对方不是websocket，则可能按http下发404之类的其他东西
    if (unlikely(is_upgrade_))
    {
        return unpack_error(L, "already upgrade");
    }

    is_upgrade_ = true;
    // https://tools.ietf.org/pdf/rfc6455.pdf Section 1.3,page 6

    const char *key_str    = nullptr;
    const char *accept_str = nullptr;

    /* 不知道当前是服务端还是客户端，两个key都查找，由上层处理 */
    const HeadField &head_field = http_info_.head_field_;
    auto key_itr                = head_field.find("Sec-WebSocket-Key");
    if (key_itr != head_field.end())
    {
        key_str = key_itr->second.c_str();
    }
    else
    {
        auto accept_itr = head_field.find("Sec-WebSocket-Accept");
        if (accept_itr != head_field.end())
        {
            accept_str = accept_itr->second.c_str();
        }
    }

    lua_pushinteger(L, PC_UPGRADE);
    lua_pushstring(L, key_str);
    lua_pushstring(L, accept_str);

    return 3;
}

const char *WebsocketPacket::make_payload(size_t &size)
{
    size = parser_->length;
    if (0 == size) return nullptr; // 没有payload(比如只发一些ctrl帧)


    char *payload = socket_->get_recv_buffer().peek_buffer(size, 1);

    // websocket的XOR算法是单个字节运算，允许在原buffer上修改
    // 如果带masking-key，则收到的body都需要用masking-key来解码才能得到原始数据
    if (parser_->flags & WS_HAS_MASK)
    {
        websocket_parser_decode(payload, payload, size, parser_);
    }

    return payload;
}

int32_t WebsocketPacket::on_frame_end()
{
    size_t size         = 0;
    const char *payload = make_payload(size);

    lua_pushinteger(L_, PC_DATA);
    lua_pushlstring(L_, payload, size);

    return 2;
}

int32_t WebsocketPacket::on_ctrl_end()
{
    size_t size         = 0;
    const char *payload = make_payload(size);

    lua_pushinteger(L_, PC_CTRL);
    lua_pushinteger(L_, parser_->flags);

    int32_t argc = 2; // 控制帧也是可以包含数据的
    if (size > 0)
    {
        argc++;
        lua_pushlstring(L_, payload, size);
    }

    return argc;
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
