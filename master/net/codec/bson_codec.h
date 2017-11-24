#ifndef __BSON_CODEC_H__
#define __BSON_CODEC_H__

#include "codec.h"

struct bson_t;
class bson_codec : public codec
{
public:
    bson_codec();
    ~bson_codec();

    void finalize();

    /* 解码数据包
     * return: <0 error,otherwise the number of parameter push to stack
     */
     int32 decode(
         lua_State *L,const char *buffer,int32 len,const cmd_cfg_t *cfg );
    /* 编码数据包
     * return: <0 error,otherwise the length of buffer
     */
    int32 encode( lua_State *L,const char **buffer,const cmd_cfg_t *cfg );

    /* 解码数据包
     * return: <0 error,otherwise the number of parameter push to stack
     */
    int32 raw_decode( lua_State *L,const char *buffer,int32 len );
    /* 编码数据包
     * return: <0 error
     */
    int32 raw_encode( lua_State *L,
        int32 unique_id,int32 ecode,int32 index,class buffer &send );
private:
    bson_t *_doc;
};

#endif /* __BSON_CODEC_H__ */
