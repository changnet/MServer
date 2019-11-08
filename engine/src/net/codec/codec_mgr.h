#pragma once

#include "codec.h"

class CodecMgr
{
public:
    ~CodecMgr();
    explicit CodecMgr();

    class Codec *get_codec(Codec::CodecType type);
    int32_t load_one_schema(Codec::CodecType type, const char *path) const;

private:
    class Codec *_codecs[Codec::CDC_MAX];
};
