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
    /* 解码数据包
     * return: <0 error,otherwise the number of parameter push to stack
     */
    int32 decode( lua_State *L,
        codec_t codec_ty,const char *buffer,int32 len,const cmd_cfg_t *cfg );
    /* 编码数据包
     * return: <0 error
     */
    int32 encode(
        lua_State *L,codec_t codec_ty,class buffer *buff,const cmd_cfg_t *cfg );
private:
    codec();
    virtual ~codec();
};

#endif /* __CODEC_H__ */
