#include "codec.h"

static class codec *codec::instance( codec_t type )
{
    if ( type < PKT_NONE || type >= PKT_MAX ) return NULL;

    static class codec list[] = 
    {
        NULL,
        new bson_codec(),
        NULL,
        new protobuf_codec()
    };

    static_assert( sizeof(list)/sizeof(list[1]) == PKT_MAX - 1,"" );

    return list[type];
}
