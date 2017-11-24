#ifndef __CODEC_H__
#define __CODEC_H__

/* socket packet codec */
class codec
{
public:
    typedef enum
    {
        CDC_NONE     = 0, // 不需要解码，比如来自http的json字符串，由上层解析
        CDC_BSON     = 1, // bson
        CDC_STREAM   = 2, // 自定义二进制流
        CDC_FLATBUF  = 3, // google FlatBuffers
        CDC_PROTOBUF = 4, // google protocol buffers

        CDC_MAX
    }codec_t;
public:
    codec() {};
    virtual ~codec() {};

    /* 解码数据包
     * return: <0 error,otherwise the number of parameter push to stack
     */
    int32 decode( lua_State *L,
        const char *buffer,int32 len,const cmd_cfg_t *cfg ) = 0;
    /* 编码数据包
     * return: <0 error,otherwise the length of buffer
     */
    int32 encode( lua_State *L,const char **buffer,const cmd_cfg_t *cfg ) = 0;

    /* 解码、编码结束，处理后续工作
     */
    void finalize() = 0;

    static class codec *instance( codec_t type );
};

#endif /* __CODEC_H__ */
