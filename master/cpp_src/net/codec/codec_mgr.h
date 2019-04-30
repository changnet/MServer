#ifndef __CODEC_MGR_H__
#define __CODEC_MGR_H__

#include "codec.h"

class codec_mgr
{
public:
    ~codec_mgr();
    explicit codec_mgr();

    class codec *get_codec( codec::codec_t type );
    int32 load_one_schema( codec::codec_t type,const char *path ) const;
private:
    class codec* _codecs[codec::CDC_MAX];
};

#endif /* __CODEC_MGR_H__ */
