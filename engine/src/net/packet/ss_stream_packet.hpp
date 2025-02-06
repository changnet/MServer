#pragma once

#include "packet.hpp"

// 服务器之间的二进制数据打包
class SsStreamPacket : public Packet
{
public:
    virtual ~SsStreamPacket();
    SsStreamPacket(class Socket *sk);

    PacketType type() const override { return PT_SSSTREAM; }

    virtual int32_t pack_clt(lua_State *L, int32_t index) override;
    virtual int32_t pack_srv(lua_State *L, int32_t index) override;

    int32_t unpack(lua_State *L, Buffer &buffer) override;
};
