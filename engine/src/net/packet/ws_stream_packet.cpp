#include <websocket_parser.h>

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

    size_t frame_size      = size + sizeof(header);
    const char *header_ctx = reinterpret_cast<const char *>(&header);
    size_t len             = websocket_calc_frame_size(flags, frame_size);

    char mask[4]             = {0};
    uint8_t mask_offset      = 0;
    Buffer &buffer           = socket_->get_send_buffer();
    // TODO 这里是不是可以用websocket_append_frame而不需要reserve
    Buffer::Transaction &&ts = buffer.flat_reserve(len, 2);

    if (flags & WS_HAS_MASK) new_masking_key(mask);

    size_t offset =
        websocket_build_frame_header(ts.ctx_, flags, mask, frame_size);
    offset += websocket_append_frame(ts.ctx_ + offset, flags, mask, header_ctx,
                                     sizeof(header), &mask_offset);
    websocket_append_frame(ts.ctx_ + offset, flags, mask, ctx, size,
                           &mask_offset);

    buffer.commit(ts, (int32_t)len);
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
    size_t size     = 0;
    const char *ctx = body_.all_to_flat_ctx(size, 1);

    if (size < sizeof(WssHeader))
    {
        unpack_error(L_, "incomplete stream data");
        return WPE_PAUSE; // unpack_error返回错误在lua处理，不需要返回WPE_ERROR;
    }

    decltype(WssHeader::cmd_) cmd = 0;
    memcpy(&cmd, ctx, sizeof(cmd));

    lua_pushinteger(L_, PC_DATA);
    lua_pushinteger(L_, cmd);

    // 这里返回ctx（包含cmd）而不是ctx + sizeof(cmd)
    // 因为脚本可能要转发整个ctx到其他线程或者其他进程
    lua_pushlightuserdata(L_, (void *)(ctx + sizeof(cmd)));
    lua_pushinteger(L_, size - sizeof(cmd));

    return WPE_PAUSE;
}
