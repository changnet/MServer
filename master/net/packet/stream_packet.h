#ifndef __STREAM_PACKET_H__
#define __STREAM_PACKET_H__

#include "packet.h"

struct base_header;
class stream_packet : public packet
{
public:
    virtual ~stream_packet();
    stream_packet( class socket *sk );

    int32 pack();
    int32 unpack();
private:
    void dispatch( const struct base_header *header );
};

#endif /* __STREAM_PACKET_H__ */
