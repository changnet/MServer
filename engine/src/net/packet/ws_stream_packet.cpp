#include "lpp/ltools.hpp"
#include "net/socket.hpp"
#include "ws_stream_packet.hpp"

#pragma pack(push, 1)
struct WssHeader
{
    uint16_t cmd_; /* 协议号 */
};
#pragma pack(pop)

WSStreamPacket::~WSStreamPacket()
{
}

WSStreamPacket::WSStreamPacket(class Socket *sk) : WebsocketPacket(sk)
{
}

int32_t WSStreamPacket::pack_any(lua_State *L, int32_t index)
{
    // 允许握手未完成就发数据，自己保证顺序
    // if ( !is_upgrade_ ) return http_packet::pack_clt( L,index );

    websocket_flags flags =
        static_cast<websocket_flags>(luaL_checkinteger(L, index));
    int32_t cmd     = luaL_checkinteger32(L, index + 1);
    const char *ctx = static_cast<const char *>(luaL_checkludata(L, index + 2));
    int32_t size    = luaL_checkinteger32(L, index + 3);
    if (size < 0) return -1;

    struct WssHeader header;
    header.cmd_ = static_cast<uint16_t>(cmd);

    char mask[4]             = {0};
    uint8_t mask_offset      = 0;
    size_t frame_size        = size + sizeof(header);
    Buffer &buffer           = socket_->get_send_buffer();

    pack_header(buffer, flags, mask, ctx, frame_size);
    pack_frame(buffer, flags, mask, reinterpret_cast<const char *>(&header),
               sizeof(header), &mask_offset);
    pack_frame(buffer, flags, mask, ctx, size, &mask_offset);
 
    socket_->flush();

    return 0;
}

int32_t WSStreamPacket::pack_clt(lua_State *L, int32_t index)
{
    return pack_any(L, index);
}

int32_t WSStreamPacket::pack_srv(lua_State *L, int32_t index)
{
    return pack_any(L, index);
}

int32_t WSStreamPacket::on_frame_end()
{
    size_t size         = 0;
    const char *payload = make_payload(size);

    if (size < sizeof(WssHeader))
    {
        unpack_error(L_, "incomplete stream data");
        return WPE_PAUSE; // unpack_error返回错误在lua处理，不需要返回WPE_ERROR;
    }

    WssHeader header;
    memcpy(&header, payload, sizeof(WssHeader));

    lua_pushinteger(L_, PC_DATA);
    lua_pushinteger(L_, header.cmd_);

    lua_pushlightuserdata(L_, (void *)(payload + sizeof(WssHeader)));
    lua_pushinteger(L_, size - sizeof(WssHeader));

    return WPE_PAUSE;
}
