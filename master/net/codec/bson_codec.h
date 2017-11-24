#ifndef __BSON_CODEC_H__
#define __BSON_CODEC_H__

#include "codec.h"

class bson_codec : public codec
{
public:
    /* 解码数据包
     * return: <0 error,otherwise the number of parameter push to stack
     */
    static int32 raw_decode( lua_State *L,const char *buffer,int32 len );
    /* 编码数据包
     * return: <0 error
     */
    static int32 raw_encode( lua_State *L,
        int32 unique_id,int32 ecode,int32 index,class buffer &send );
};

#endif /* __BSON_CODEC_H__ */
