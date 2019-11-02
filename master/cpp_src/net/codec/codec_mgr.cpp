#include "codec_mgr.h"

#include "bson_codec.h"
#include "protobuf_codec.h"
#include "flatbuffers_codec.h"

codec_mgr::codec_mgr()
{
    memset( _codecs,0,sizeof(_codecs) );

    _codecs[codec::CDC_BSON] = new class bson_codec();
    _codecs[codec::CDC_FLATBUF] = new class flatbuffers_codec();
    _codecs[codec::CDC_PROTOBUF] = new class protobuf_codec();
}

codec_mgr::~codec_mgr()
{
    for (int32_t idx = 0;idx < codec::CDC_MAX;idx ++ )
    {
        if ( _codecs[idx] )
        {
            delete _codecs[idx];
            _codecs[idx] = NULL;
        }
    }
}

int32_t codec_mgr::load_one_schema( codec::codec_t type,const char *path ) const
{
    return  NULL == _codecs[type] ? -1 : _codecs[type]->load_path( path );
}

class codec *codec_mgr::get_codec( codec::codec_t type )
{
    if ( type < codec::CDC_NONE || type >= codec::CDC_MAX ) return NULL;

    return _codecs[type];
}
