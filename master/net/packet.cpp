#include "packet.h"
#include "buffer.h"

#if defined FLATBUFFERS_PARSE
    #define decoder_t lflatbuffers
    #include "packet_flatbuffers.cpp"
#elif defined PROTOBUF_PARSE
    #define decoder_t lprotobuf
    #include "packet_protobuf.cpp"
#elif defined STREAM_PARSE
    #include "packet_stream.cpp"
#else
    #error no header parse specify
#endif

#include "packet_rpc.cpp"

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

packet::packet()
{
    _decoder = new decoder_t();
}

packet::~packet()
{
    /* delete一个void指针不会调用析构函数 */
    delete (decoder_t *)_decoder;
    _decoder              = NULL;
}

