#ifndef __PROTOBUF_CODEC_H__
#define __PROTOBUF_CODEC_H__

#include "codec.h"

class lprotobuf;
class protobuf_codec : public codec
{
public:
    protobuf_codec();
    ~protobuf_codec();

    void finalize();

    /* 解码数据包
     * return: <0 error,otherwise the number of parameter push to stack
     */
    int32 decode(
         lua_State *L,const char *buffer,int32 len,const cmd_cfg_t *cfg );
    /* 编码数据包
     * return: <0 error,otherwise the length of buffer
     */
    int32 encode(
        lua_State *L,int32 index,const char **buffer,const cmd_cfg_t *cfg );
private:
    class lprotobuf *_lprotobuf;
};

#endif /* __PROTOBUF_CODEC_H__ */
