#include "stream_packet.h"

stream_packet::stream_packet()
{
}

stream_packet::~stream_packet()
{
}

void remove( class buffer &buff,int32 length )
{
    buff.subtract( length );
}

int32 stream_packet::parser( class buffer &buff )
{
    return 0;
}

int32 stream_packet::deparser( class buffer &buff )
{
    return 0;
}
