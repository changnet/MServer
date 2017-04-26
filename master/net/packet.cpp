#include "packet.h"

#if defined FLATBUFFERS_PARSE
    #include "packet_flatbuffers.cpp"
#elif defined PROTOBUF_PARSE
    #include "packet_protobuf.cpp"
#elif defined STREAM_PARSE
    #include "packet_stream.cpp"
#else
    #error no header parse specify
#endif