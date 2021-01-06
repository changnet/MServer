#include "codec_mgr.hpp"

#include "bson_codec.hpp"
#include "flatbuffers_codec.hpp"
#include "protobuf_codec.hpp"

CodecMgr::CodecMgr()
{
    memset(_codecs, 0, sizeof(_codecs));

    _codecs[Codec::CDC_BSON]     = new class BsonCodec();
    _codecs[Codec::CDC_FLATBUF]  = new class FlatbuffersCodec();
    _codecs[Codec::CDC_PROTOBUF] = new class ProtobufCodec();
}

CodecMgr::~CodecMgr()
{
    for (int32_t idx = 0; idx < Codec::CDC_MAX; idx++)
    {
        if (_codecs[idx])
        {
            delete _codecs[idx];
            _codecs[idx] = NULL;
        }
    }
}

int32_t CodecMgr::load_one_schema(Codec::CodecType type, const char *path) const
{
    return NULL == _codecs[type] ? -1 : _codecs[type]->load_path(path);
}

class Codec *CodecMgr::get_codec(Codec::CodecType type)
{
    if (type < Codec::CDC_NONE || type >= Codec::CDC_MAX) return NULL;

    return _codecs[type];
}
