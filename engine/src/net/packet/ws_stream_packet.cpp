#include <websocket_parser.h>

#include "lpp/ltools.hpp"
#include "system/static_global.hpp"
#include "net/socket.hpp"
#include "ws_stream_packet.hpp"

#pragma pack(push, 1)
struct WSHeader
{
    uint16_t cmd_; /* 协议号 */
};
#pragma pack(pop)

WSStreamPacket::~WSStreamPacket() {}

WSStreamPacket::WSStreamPacket(class Socket *sk) : WebsocketPacket(sk) {}


int32_t WSStreamPacket::pack_clt(lua_State *L, int32_t index)
{
    // 允许握手未完成就发数据，自己保证顺序
    // if ( !is_upgrade_ ) return http_packet::pack_clt( L,index );

    int32_t cmd = luaL_checkinteger32(L, index);

    websocket_flags flags =
        static_cast<websocket_flags>(luaL_checkinteger(L, index + 1));

    const char *ctx = static_cast <const char *>(luaL_checkludata(L, index + 2));
    int32_t size    = luaL_checkinteger32(L, index + 3);
    if (size < 0) return -1;

    struct WSHeader header;
    header.cmd_ = static_cast<uint16_t>(cmd);

    size_t frame_size = size + sizeof(header);
    const char *header_ctx = reinterpret_cast<const char *>(&header);
    size_t len             = websocket_calc_frame_size(flags, frame_size);

    char mask[4]             = {0};
    uint8_t mask_offset      = 0;
    Buffer &buffer           = socket_->get_send_buffer();
    Buffer::Transaction &&ts = buffer.flat_reserve(len);
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

int32_t WSStreamPacket::pack_srv(lua_State *L, int32_t index)
{
    // 允许握手未完成就发数据，自己保证顺序
    // if ( !is_upgrade_ ) return luaL_error( L,"websocket not upgrade" );

    int32_t cmd = luaL_checkinteger32(L, index);
    websocket_flags flags =
        static_cast<websocket_flags>(luaL_checkinteger(L, index + 1));

    const char *ctx = (const char *)luaL_checkludata(L, index + 2);
    size_t size     = luaL_checkinteger(L, index + 3);
    if (size < 0) return -1;

    struct WSHeader header;
    header.cmd_ = static_cast<uint16_t>(cmd);

    size_t frame_size = size + sizeof(header);

    const char *header_ctx = reinterpret_cast<const char *>(&header);
    size_t len             = websocket_calc_frame_size(flags, frame_size);

    char mask[4] = {0};
    new_masking_key(mask);

    uint8_t mask_offset = 0;
    Buffer &buffer      = socket_->get_send_buffer();
    Buffer::Transaction &&ts = buffer.flat_reserve(len);
    size_t offset = websocket_build_frame_header(ts.ctx_, flags, mask, frame_size);
    offset += websocket_append_frame(ts.ctx_ + offset, flags, mask, header_ctx,
                                     sizeof(header), &mask_offset);
    websocket_append_frame(ts.ctx_ + offset, flags, mask, ctx, size,
                           &mask_offset);

    buffer.commit(ts, (int32_t)len);
    socket_->flush();

    return 0;
}

/* 数据帧完成 */
int32_t WSStreamPacket::on_frame_end()
{
    size_t size = 0;
    const char *ctx = body_.all_to_flat_ctx(size);

    if (size < sizeof(WSHeader))
    {
        unpack_error(L_, "incomplete stream data");
        return WPE_PAUSE; // unpack_error返回错误在lua处理，不需要返回WPE_ERROR;
    }

    decltype(WSHeader::cmd_) cmd = 0;
    memcpy(&cmd, ctx, sizeof(cmd));

    lua_pushinteger(L_, PC_DATA);
    lua_pushinteger(L_, cmd);
    lua_pushlightuserdata(L_, (void *)ctx);
    lua_pushinteger(L_, size);

    return WPE_PAUSE;
}
