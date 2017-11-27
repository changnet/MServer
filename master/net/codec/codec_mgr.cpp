#include "codec_mgr.h"

#include "bson_codec.h"
#include "protobuf_codec.h"
#include "flatbuffers_codec.h"

class codec_mgr *codec_mgr::_codec_mgr = NULL;

codec_mgr::codec_mgr()
{
    memset( _codecs,0,sizeof(_codecs) );

    _codecs[codec::CDC_BSON] = new class bson_codec();
    _codecs[codec::CDC_FLATBUF] = new class flatbuffers_codec();
    _codecs[codec::CDC_PROTOBUF] = new class protobuf_codec();
}

codec_mgr::~codec_mgr()
{
    for (int32 idx = 0;codec::CDC_MAX;idx ++ )
    {
        if ( _codecs[idx] )
        {
            delete _codecs[idx];
            _codecs[idx] = NULL;
        }
    }
}

class codec_mgr *codec_mgr::instance()
{
    if ( !_codec_mgr )
    {
        _codec_mgr = new class codec_mgr();
    }

    return _codec_mgr;
}

void codec_mgr::uninstance()
{
    delete _codec_mgr;
    _codec_mgr = NULL;
}

int32 codec_mgr::load_schema( const char *path )
{
    for (int32 idx = 0;codec::CDC_MAX;idx ++ )
    {
        if ( _codecs[idx] )
        {
            _codecs[idx]->load_path( path );
        }
    }
    return 0;
}

class codec *codec_mgr::get_codec( codec::codec_t type )
{
    if ( type < codec::CDC_NONE || type >= codec::CDC_MAX ) return NULL;

    return _codecs[type];
}
