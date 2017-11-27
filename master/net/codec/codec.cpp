#include "codec.h"

#include "bson_codec.h"
#include "protobuf_codec.h"
#include "flatbuffers_codec.h"

int32 codec::load_schema()
{
    return 0;
}

class codec *codec::instance( codec_t type )
{
    if ( type < CDC_NONE || type >= CDC_MAX ) return NULL;

    static class codec *list[] = 
    {
        NULL,
        new bson_codec(),
        NULL,
        new protobuf_codec()
    };

    static_assert( sizeof(list)/sizeof(list[1]) == CDC_MAX - 1,"" );

    return list[type];
}
