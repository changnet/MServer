#ifndef __CODEC_MGR_H__
#define __CODEC_MGR_H__

#include "codec.h"

class codec_mgr
{
public:
    static void uninstance();
    static class codec_mgr *instance();

    int32 load_schema( const char *path );
    class codec *get_codec( codec::codec_t type );
private:
    codec_mgr();
    ~codec_mgr();

    class codec* _codecs[codec::CDC_MAX];

    static class codec_mgr *_codec_mgr;
};

#endif /* __CODEC_MGR_H__ */
