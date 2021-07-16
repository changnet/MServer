#pragma once

#include "codec.hpp"

class CodecMgr
{
public:
    ~CodecMgr();
    explicit CodecMgr();

    void reset(Codec::CodecType type) const;
    class Codec *get_codec(Codec::CodecType type);
    int32_t load_one_schema(Codec::CodecType type, const char *path) const;
    int32_t load_one_schema_file(Codec::CodecType type, const char *path) const;

private:
    class Codec *_codecs[Codec::CT_MAX];
};
