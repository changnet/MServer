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
    ws_packet->try_set_payload(at);

    return 0;
}

// 数据帧完成
static int32_t on_frame_end(struct websocket_parser *parser)
{
    assert(parser && parser->data);

    class WebsocketPacket *ws_packet =
        static_cast<class WebsocketPacket *>(parser->data);

    /* https://tools.ietf.org/html/rfc6455#section-5.5
     * opcode并不是按位来判断的，而是按顺序1、2、3、4...
     * 它们是互斥的，只能存在其中一个,我们只需要判断最高位即可(Control frames are
     * identified by opcodes where the most significant bit of the opcode is 1)
     *
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

    require_len_ = 0;
    to_remove_   = 0;
    head_len_    = 0;
    payload_     = nullptr;
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
    // websocket的header需要2~14字节，如果space只剩下几字节要额外处理
    static constexpr int32_t min_space = 32;

    Buffer::Chunk *tail       = buffer.get_back();

    int32_t space     = 0;
    char *buffer_ptr = tail->write_ptr(space);
    if (space >= min_space)
    {
        size_t offset =
            websocket_build_frame_header(buffer_ptr, flags, mask, data_len);
        buffer.add_used(tail, offset);
    }
    else
    {
        char header[min_space];
        size_t offset =
            websocket_build_frame_header(header, flags, mask, data_len);
        buffer.append(header, offset);
    }
}

void WebsocketPacket::pack_frame(Buffer& buffer, websocket_flags flags,
    const char mask[4],
    const char* data, size_t data_len, uint8_t* mask_offset)
{
    if (0 == data_len) return;
    Buffer::Chunk *tail = buffer.get_back();

    int32_t space    = 0;
    char *buffer_ptr = tail->write_ptr(space);

    if (space > 0)
    {
        size_t pack_len = std::min((size_t)space, data_len);
        websocket_append_frame(buffer_ptr, flags, mask, data, pack_len,
                               mask_offset);
        data += pack_len;
        data_len -= pack_len;
        buffer.add_used(tail, pack_len);
    }

    static const size_t LARGE_SIZE = Buffer::CHUNK_SIZE * 8;
    while (data_len > 0)
    {
        size_t alloc_size = data_len > LARGE_SIZE ? data_len : Buffer::CHUNK_SIZE;

        Buffer::Chunk *new_chk = buffer.allocate_chunk(alloc_size);

        size_t pack_len = std::min((size_t)new_chk->capacity_, data_len);
        websocket_append_frame(new_chk->data(), flags, mask, data, pack_len,
                               mask_offset);
        data += pack_len;
        data_len -= pack_len;
        buffer.append_chunk(new_chk, (int32_t)pack_len);
    }
}

int32_t WebsocketPacket::pack_any(lua_State *L, int32_t index)
{
    // 允许握手未完成就发数据，自己保证顺序
    // if ( !is_upgrade_ ) return http_packet::pack_clt( L,index );

    websocket_flags flags =
        static_cast<websocket_flags>(luaL_checkinteger(L, index));

    size_t size     = 0;
    const char *ctx = luaL_optlstring(L, index + 1, nullptr, &size);
    // if ( !ctx ) return 0; // 允许发送空包

    char mask[4] = {0}; /* 服务器发往客户端并不需要mask */
    if (flags & WS_HAS_MASK) new_masking_key(mask);

    Buffer &buffer = socket_->get_send_buffer();

    pack_header(buffer, flags, mask, ctx, size);

    uint8_t mask_offset = 0;
    pack_frame(buffer, flags, mask, ctx, size, &mask_offset);

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

int32_t WebsocketPacket::unpack(lua_State *L, Buffer &buffer)
{
    /* 未握手时，由http处理
     * 握手成功后，http中止处理，未处理的数据仍在buffer中，由websocket继续处理
     */
    if (!is_upgrade_) return HttpPacket::unpack(L, buffer);

    if (to_remove_) // 上一个协议的数据，开始解析下一个协议前先删除旧协议
    {
        buffer.remove(to_remove_);
        to_remove_ = 0;
        payload_   = nullptr;
    }
    else if (require_len_ > 0)
    {
        // 已经计算出payload的大小，如果收到的数据不完整先不解析
        // TODO 本来解析也没什么问题，只是buffer是一个多线程队列，效率很低
        // 尤其是不拷贝数据，解析时要跳过部分chunk效率更慢
        if (buffer.get_used_size() < require_len_)
        {
            return unpack_return(L, PC_MORE);
        }
    }
    else
    {
        payload_ = nullptr;
    }

    e_ = 0; // 重置上一次解析错误
    L_ = L;

    size_t size     = 0;
    int32_t old_top = lua_gettop(L);

    // 只取单个chunk的数据进行解析而不是把多个chunk复制到一个连续缓冲区再解析
    const char *ctx = buffer.get_front_data(size);
    if (size == 0) return unpack_return(L, PC_MORE);

    // websocket_parser_execute把数据全当二进制处理，没有错误返回
    // 解析过程中，如果settings中回调返回非0值，则中断解析并返回已解析的字符数

    size_t nparser = websocket_parser_execute(parser_, &settings, ctx, size);

    assert(0 != nparser);
    // 未解析到payload的说明都是header，不需要保留
    // 保证会导致buffer.get_front_data需要跳过上次的数据，非常麻烦
    if (!payload_)
    {
        assert(0 == to_remove_);
        buffer.remove(nparser);
    }

    int32_t e = parser_->error;
    if (WPE_OK == e)
    {
        // parser_解析完head时不提供已经任何方法获取head的长度，不在on_frame_body
        // 复制数据的话，这里只能手动计算长度
        if (payload_ && 0 == require_len_)
        {
            head_len_ = payload_ - ctx;
            assert(head_len_ > 0 && parser_->length > 0);
            require_len_ = head_len_ + parser_->length;
        }
        // 一个message的数据包含在多个缓冲区，继续解析
        return unpack(L, buffer);
    }
    else if (WPE_PAUSE == e)
    {
        // 成功解析出数据，数据已经在on_frame_end中设置到lua的堆栈

        // 与llhttp不一样，websocket_parser不能用pause停下来，否则state状态不对
        // 类似llhttp_resume，这里重置state让它能pause并继续解析
        parser_->state  = 0;
        int32_t new_top = lua_gettop(L);
        assert(new_top - old_top > 0);
        return new_top - old_top;
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
    if (0 == size)
    {
        assert(!payload_);
        return nullptr; // 没有payload(比如只发一些ctrl帧)
    }

    const char *payload = payload_;

    // 0 != require_len_表示数据不在同一个chunk中，需要合并
    if (0 != require_len_)
    {
        payload = socket_->get_recv_buffer().peek_buffer(size, 1);
        payload += head_len_;
    }
    assert(payload);
    to_remove_   = require_len_;
    require_len_ = 0;

    // websocket的XOR算法是单个字节运算，允许在原buffer上修改
    // 如果带masking-key，则收到的body都需要用masking-key来解码才能得到原始数据
    if (parser_->flags & WS_HAS_MASK)
    {
        websocket_parser_decode(const_cast<char *>(payload), payload, size,
                                parser_);
    }

    return payload;
}

int32_t WebsocketPacket::on_frame_end()
{
    size_t size         = 0;
    const char *payload = make_payload(size);

    lua_pushinteger(L_, PC_DATA);
    lua_pushlstring(L_, payload, size);

    return WPE_PAUSE;
}

int32_t WebsocketPacket::on_ctrl_end()
{
    size_t size         = 0;
    const char *payload = make_payload(size);

    lua_pushinteger(L_, PC_CTRL);
    lua_pushinteger(L_, parser_->flags);

    // 控制帧也是可以包含数据的
    if (size > 0) lua_pushlstring(L_, payload, size);

    return WPE_PAUSE;
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
