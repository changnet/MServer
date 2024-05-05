#pragma once

#include "websocket_packet.hpp"

class WSStreamPacket : public WebsocketPacket
{
public:
    ~WSStreamPacket();
    WSStreamPacket(class Socket *sk);

    PacketType type() const { return PT_WSSTREAM; }

private:
    int32_t sc_command();
    /* 回调 client to server 的数据包 */
    int32_t cs_command(int32_t cmd, const char *ctx, size_t size);
    int32_t do_pack_clt(int32_t raw_flags, int32_t cmd, uint16_t ecode,
                        const char *ctx, size_t size);
};
