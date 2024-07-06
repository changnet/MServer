#pragma once

#include "net/net_header.hpp"
#include "packet.hpp"

struct base_header;
struct c2s_header;
struct s2s_header;
struct CmdCfg;

class StreamPacket : public Packet
{
public:
    virtual ~StreamPacket();
    StreamPacket(class Socket *sk);

    PacketType type() const { return PT_STREAM; }

    int32_t unpack(Buffer &buffer);

private:
    void dispatch(const struct base_header *header);

};
