#pragma once

#include "codec.h"

class codec_mgr
{
public:
    ~codec_mgr();
    explicit codec_mgr();

    class codec *get_codec( codec::codec_t type );
    int32_t load_one_schema( codec::codec_t type,const char *path ) const;
private:
    class codec* _codecs[codec::CDC_MAX];
};
