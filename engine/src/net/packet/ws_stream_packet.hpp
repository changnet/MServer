#pragma once

#include "websocket_packet.hpp"

class WSStreamPacket : public WebsocketPacket
{
public:
    ~WSStreamPacket();
    WSStreamPacket(class Socket *sk);

    PacketType type() const { return PT_WSSTREAM; }

    /* 打包服务器发往客户端数据包
     * return: <0 error;>=0 success
     */
    virtual int32_t pack_clt(lua_State *L, int32_t index);
    /* 打包客户端发往服务器数据包
     * return: <0 error;>=0 success
     */
    virtual int32_t pack_srv(lua_State *L, int32_t index);

    /* 数据帧完成 */
    virtual int32_t on_frame_end();

    int32_t raw_pack_clt(int32_t cmd, uint16_t ecode, const char *ctx,
                         size_t size);

private:
    int32_t sc_command();
    int32_t cs_command(int32_t cmd, const char *ctx, size_t size);
    int32_t do_pack_clt(int32_t raw_flags, int32_t cmd, uint16_t ecode,
                        const char *ctx, size_t size);
};
