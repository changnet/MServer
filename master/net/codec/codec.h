#ifndef __CODEC_H__
#define __CODEC_H__

/* socket packet codec */
class codec
{
public:
    typedef enum
    {
        CDC_NONE     = 0, // 不需要解码，比如来自http的json字符串，由上层解析
        CDC_STREAM   = 1, // 自定义二进制流
        CDC_FLATBUF  = 2, // google FlatBuffers
        CDC_PROTOBUF = 3, // google protocol buffers

        CDC_MAX
    }codec_t;
public:
    int32 decode( lua_State *L,
        codec_t codec_ty,const char *buffer,int32 len,const cmd_cfg_t *cfg );
    int32 encode( lua_State *L,
        codec_t codec_ty,const char *buffer,int32 len,const cmd_cfg_t *cfg );
private:
    codec();
    virtual ~codec();
};

#endif /* __CODEC_H__ */
