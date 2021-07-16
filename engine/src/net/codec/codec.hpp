#pragma once

#include "../../global/global.hpp"

/* socket packet codec */

struct lua_State;
struct CmdCfg;

class Codec
{
public:
    enum CodecType
    {
        CT_NONE     = 0, ///< 协议编码方式，不编码，比如来自http的字符串，由上层解析
        CT_BSON     = 1, ///< 协议编码方式，bson，rpc使用
        CT_STREAM   = 2, ///< 协议编码方式，自定义二进制流
        CT_FLATBUF  = 3, ///< 协议编码方式，google FlatBuffers
        CT_PROTOBUF = 4, ///< 协议编码方式，google protocol buffers

        CT_MAX ///< 协议编码方式最大值
    } ;

public:
    Codec() {}
    virtual ~Codec() {}

    /**
     * 重置所有协议描述文件，用于热更
     */
    virtual void reset() = 0;
    /**
     * 解码数据包
     * @return  <0 error,otherwise the number of parameter push to stack
     */
    virtual int32_t decode(lua_State *L, const char *buffer, size_t len,
                           const CmdCfg *cfg) = 0;
    /**
     * 编码数据包
     * return  <0 error,otherwise the length of buffer
     */
    virtual int32_t encode(lua_State *L, int32_t index, const char **buffer,
                           const CmdCfg *cfg) = 0;

    /**
     * 解码、编码结束，处理后续工作
     */
    virtual void finalize() = 0;

    /**
     * 从目录加载所有schema文件
     * @param path schema文件所在路径
     */
    virtual int32_t load_path(const char *path) = 0;

    /**
     * 加载单个schema文件
     * @param path schema文件所在路径
     */
    virtual int32_t load_file(const char *path) = 0;
};
