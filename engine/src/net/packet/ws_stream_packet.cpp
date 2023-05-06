#include <websocket_parser.h>

#include "../../lua_cpplib/ltools.hpp"
#include "../../system/static_global.hpp"
#include "../socket.hpp"
#include "ws_stream_packet.hpp"

WSStreamPacket::~WSStreamPacket() {}

WSStreamPacket::WSStreamPacket(class Socket *sk) : WebsocketPacket(sk) {}

/* 打包服务器发往客户端数据包
 * pack_clt( cmd,errno,flags,ctx )
 * return: <0 error;>=0 success
 */
int32_t WSStreamPacket::pack_clt(lua_State *L, int32_t index)
{
    STAT_TIME_BEG();
    // 允许握手未完成就发数据，自己保证顺序
    // if ( !_is_upgrade ) return http_packet::pack_clt( L,index );

    int32_t cmd    = luaL_checkinteger32(L, index);

    websocket_flags flags =
        static_cast<websocket_flags>(luaL_checkinteger(L, index + 1));

    const char *ctx = static_cast <const char *>(luaL_checkludata(L, index + 2));
    int32_t size    = luaL_checkinteger32(L, index + 3);
    if (size < 0) return -1;

    if (size > MAX_PACKET_LEN)
    {
        return luaL_error(L, "buffer size over MAX_PACKET_LEN");
    }

    if (do_pack_clt(flags, cmd, ecode, ctx, size) < 0)
    {
        return luaL_error(L, "can not do_pack_clt");
    }

    PKT_STAT_ADD(SPT_SCPK, cmd, int32_t(size + sizeof(struct s2c_header)),
                 STAT_TIME_END());
    return 0;
}

/* 打包客户端发往服务器数据包
 * pack_srv( cmd,flags,ctx )
 * return: <0 error;>=0 success
 */
int32_t WSStreamPacket::pack_srv(lua_State *L, int32_t index)
{
    STAT_TIME_BEG();
    // 允许握手未完成就发数据，自己保证顺序
    // if ( !_is_upgrade ) return luaL_error( L,"websocket not upgrade" );

    int32_t cmd = luaL_checkinteger32(L, index);
    websocket_flags flags =
        static_cast<websocket_flags>(luaL_checkinteger(L, index + 1));

    const char *ctx = luaL_checkludata(L, index + 2);
    int32_t size    = luaL_checkinteger(L, index + 3);
    if (size < 0) return -1;

    if (size > MAX_PACKET_LEN)
    {
        return luaL_error(L, "buffer size over MAX_PACKET_LEN:%d", cmd);
    }

    struct c2s_header c2sh;
    SET_HEADER_LENGTH(c2sh, size, cmd, SET_LENGTH_FAIL_ENCODE);
    c2sh._cmd = static_cast<uint16_t>(cmd);

    size_t frame_size  = size + sizeof(c2sh);

    const char *header_ctx = reinterpret_cast<const char *>(&c2sh);
    size_t len             = websocket_calc_frame_size(flags, frame_size);

    char mask[4] = {0};
    new_masking_key(mask);

    uint8_t mask_offset = 0;
    Buffer &buffer      = _socket->get_send_buffer();
    Buffer::Transaction &&ts = buffer.flat_reserve(len);
    size_t offset = websocket_build_frame_header(ts._ctx, flags, mask, frame_size);
    offset += websocket_append_frame(ts._ctx + offset, flags, mask, header_ctx,
                                     sizeof(c2sh), &mask_offset);
    websocket_append_frame(ts._ctx + offset, flags, mask, ctx, size,
                           &mask_offset);

    buffer.commit(ts, (int32_t)len);
    _socket->flush();

    PKT_STAT_ADD(SPT_CSPK, cmd, int32_t(c2sh._length), STAT_TIME_END());

    return 0;
}

/* 数据帧完成 */
int32_t WSStreamPacket::on_frame_end()
{
    Socket::ConnType conn_ty = _socket->conn_type();
    /* 客户端到服务器的连接(CSCN)收到的是服务器发往客户端的数据包(sc_command) */
    if (Socket::CT_CSCN == conn_ty)
    {
        return sc_command();
    }

    static const class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

    /* 服务器收到的包，看要不要转发 */
    size_t data_size     = 0;
    const char *data_ctx = _body.all_to_flat_ctx(data_size);
    if (data_size < sizeof(struct c2s_header))
    {
        ELOG("ws_stream_packet on_frame_end packet incomplete");
        return 0;
    }
    const struct c2s_header *header =
        reinterpret_cast<const struct c2s_header *>(data_ctx);

    uint16_t cmd = header->_cmd;
    if (data_size < header->_length)
    {
        ELOG("ws_stream_packet on_frame_end packet length error:%d",
             (int32_t)cmd);
        return 0;
    }

    size_t size     = data_size - sizeof(*header);
    const char *ctx = reinterpret_cast<const char *>(header + 1);

    return cs_command(cmd, ctx, size);
}

/* 回调server to client的数据包 */
int32_t WSStreamPacket::sc_command()
{
    static lua_State *L = StaticGlobal::state();

    assert(0 == lua_gettop(L));

    size_t data_size     = 0;
    const char *data_ctx = _body.all_to_flat_ctx(data_size);
    if (data_size < sizeof(struct s2c_header))
    {
        ELOG("ws_stream_packet sc_command packet incomplete");
        return 0;
    }

    const struct s2c_header *header =
        reinterpret_cast<const struct s2c_header *>(data_ctx);

    int cmd = header->_cmd;
    if (data_size < header->_length)
    {
        ELOG("ws_stream_packet sc_command packet length error:%d", cmd);
        return 0;
    }

    size_t size     = data_size - sizeof(*header);
    const char *ctx = reinterpret_cast<const char *>(header + 1);

    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, "on_sc_pkt");
    lua_pushinteger(L, _socket->conn_id());
    lua_pushinteger(L, cmd);
    lua_pushlightuserdata(L, const_cast<char *>(ctx));
    lua_pushinteger(L, size);

    if (EXPECT_FALSE(LUA_OK != lua_pcall(L, 4, 0, 1)))
    {
        ELOG("websocket stream on_sc_pkt:%s", lua_tostring(L, -1));
    }

    lua_settop(L, 0); /* remove traceback */

    return _socket->fd() < 0 ? -1 : 0;
}

int32_t WSStreamPacket::cs_command(int32_t cmd, const char *ctx, size_t size)
{
    static lua_State *L = StaticGlobal::state();

    assert(0 == lua_gettop(L));

    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, "on_cs_pkt");
    lua_pushinteger(L, _socket->conn_id());
    lua_pushinteger(L, cmd);
    lua_pushlightuserdata(L, const_cast<char *>(ctx));
    lua_pushinteger(L, size);

    if (EXPECT_FALSE(LUA_OK != lua_pcall(L, 4, 0, 1)))
    {
        ELOG("websocket stream on_cs_pkt:%s", lua_tostring(L, -1));
    }

    lua_settop(L, 0); /* remove traceback */

    return _socket->fd() < 0 ? -1 : 0;
}

int32_t WSStreamPacket::raw_pack_clt(int32_t cmd, uint16_t ecode,
                                     const char *ctx, size_t size)
{
    // TODO: 这个flags在通用的服务器交互中传不过来，暂时hard-code.
    // 后面如有需求，再多加一字段传过来
    websocket_flags flags =
        static_cast<websocket_flags>(WS_OP_BINARY | WS_FINAL_FRAME);

    return do_pack_clt(flags, cmd, ecode, ctx, size);
}

int32_t WSStreamPacket::do_pack_clt(int32_t raw_flags, int32_t cmd,
                                    uint16_t ecode, const char *ctx, size_t size)
{
    struct s2c_header s2ch;
    SET_HEADER_LENGTH(s2ch, size, cmd, SET_LENGTH_FAIL_RETURN);
    s2ch._cmd   = static_cast<uint16_t>(cmd);
    s2ch._errno = ecode;

    size_t frame_size  = size + sizeof(s2ch);

    websocket_flags flags  = static_cast<websocket_flags>(raw_flags);
    const char *header_ctx = reinterpret_cast<const char *>(&s2ch);
    size_t len             = websocket_calc_frame_size(flags, frame_size);

    char mask[4]        = {0};
    uint8_t mask_offset = 0;
    Buffer &buffer      = _socket->get_send_buffer();
    Buffer::Transaction &&ts = buffer.flat_reserve(len);
    size_t offset = websocket_build_frame_header(ts._ctx, flags, mask, frame_size);
    offset += websocket_append_frame(ts._ctx + offset, flags, mask, header_ctx,
                                     sizeof(s2ch), &mask_offset);
    websocket_append_frame(ts._ctx + offset, flags, mask, ctx, size,
                           &mask_offset);

    buffer.commit(ts, (int32_t)len);
    _socket->flush();

    return 0;
}
