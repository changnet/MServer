#pragma once

#include "packet.hpp"

class StreamPacket : public Packet
{
public:
    virtual ~StreamPacket();
    StreamPacket(class Socket *sk);

    PacketType type() const override { return PT_STREAM; }

    virtual int32_t pack_clt(lua_State *L, int32_t index) override;
    virtual int32_t pack_srv(lua_State *L, int32_t index) override;

    int32_t unpack(Buffer &buffer) override;

private:

};
