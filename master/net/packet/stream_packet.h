#ifndef __STREAM_PACKET_H__
#define __STREAM_PACKET_H__

#include "packet.h"

class stream_packet : public packet
{
public:
    virtual ~stream_packet();
    stream_packet( class socket *sk );

    int32 parser();
    int32 deparser();
};

#endif /* __STREAM_PACKET_H__ */
