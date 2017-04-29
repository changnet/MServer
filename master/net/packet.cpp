#include "packet.h"
#include "stream_socket.h"

#if defined FLATBUFFERS_PARSE
    #include "packet_flatbuffers.cpp"
#elif defined PROTOBUF_PARSE
    #include "packet_protobuf.cpp"
#elif defined STREAM_PARSE
    #include "packet_stream.cpp"
#else
    #error no header parse specify
#endif

class packet *packet::_packet = NULL;

void packet::uninstance()
{
    if ( _packet )
    {
        delete _packet;
    }
    _packet = NULL;
}

class packet *packet::instance()
{
    if ( !_packet )
    {
        _packet = new class packet();
    }

    return _packet;
}

