#ifndef __STREAM_PACKET_H__
#define __STREAM_PACKET_H__

#include "packet.h"

class stream_packet : public packet
{
public:
    stream_packet();
    virtual ~stream_packet();

    int32 parser( class buffer &buff );
    int32 deparser( class buffer &buff );
    void remove( class buffer &buff,int32 length );
};

#endif /* __STREAM_PACKET_H__ */
