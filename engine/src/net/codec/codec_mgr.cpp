#include "codec_mgr.hpp"

#include "bson_codec.hpp"
#include "flatbuffers_codec.hpp"
#include "protobuf_codec.hpp"

CodecMgr::CodecMgr()
{
    memset(_codecs, 0, sizeof(_codecs));

    _codecs[Codec::CT_BSON]     = new class BsonCodec();
    _codecs[Codec::CT_FLATBUF]  = new class FlatbuffersCodec();
    _codecs[Codec::CT_PROTOBUF] = new class ProtobufCodec();
}

CodecMgr::~CodecMgr()
{
    for (int32_t idx = 0; idx < Codec::CT_MAX; idx++)
    {
        if (_codecs[idx])
        {
            delete _codecs[idx];
            _codecs[idx] = nullptr;
        }
    }
}

void CodecMgr::reset(Codec::CodecType type) const
{
    if (_codecs[type]) _codecs[type]->reset();
}

int32_t CodecMgr::load_one_schema(Codec::CodecType type, const char *path) const
{
    return nullptr == _codecs[type] ? -1 : _codecs[type]->load_path(path);
}

int32_t CodecMgr::load_one_schema_file(Codec::CodecType type, const char *path) const
{
    return nullptr == _codecs[type] ? -1 : _codecs[type]->load_file(path);
}

class Codec *CodecMgr::get_codec(Codec::CodecType type)
{
    if (type < Codec::CT_NONE || type >= Codec::CT_MAX) return nullptr;

    return _codecs[type];
}
