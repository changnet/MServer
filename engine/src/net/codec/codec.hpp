#pragma once

#include "../../global/global.hpp"

/* socket packet codec */

struct lua_State;
struct CmdCfg;

class Codec
{
public:
    typedef enum
    {
        CDC_NONE     = 0, ///< 协议编码方式，不编码，比如来自http的字符串，由上层解析
        CDC_BSON     = 1, ///< 协议编码方式，bson，rpc使用
        CDC_STREAM   = 2, ///< 协议编码方式，自定义二进制流
        CDC_FLATBUF  = 3, ///< 协议编码方式，google FlatBuffers
        CDC_PROTOBUF = 4, ///< 协议编码方式，google protocol buffers

        CDC_MAX ///< 协议编码方式最大值
    } CodecType;

public:
    Codec() {}
    virtual ~Codec() {}

    /* 解码数据包
     * return: <0 error,otherwise the number of parameter push to stack
     */
    virtual int32_t decode(lua_State *L, const char *buffer, int32_t len,
                           const CmdCfg *cfg) = 0;
    /* 编码数据包
     * return: <0 error,otherwise the length of buffer
     */
    virtual int32_t encode(lua_State *L, int32_t index, const char **buffer,
                           const CmdCfg *cfg) = 0;

    /* 解码、编码结束，处理后续工作
     */
    virtual void finalize() = 0;

    /* 加载schema文件
     * @path: schema文件所在路径
     */
    virtual int32_t load_path(const char *path) = 0;
};
