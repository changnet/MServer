#pragma once

#include "codec.hpp"

class lprotobuf;
class ProtobufCodec : public Codec
{
public:
    ProtobufCodec();
    ~ProtobufCodec();

    void finalize();
    void reset();
    int32_t load_path(const char *path) override;
    int32_t load_file(const char *path) override;

    /* 解码数据包
     * return: <0 error,otherwise the number of parameter push to stack
     */
    int32_t decode(lua_State *L, const char *buffer, size_t len,
                   const CmdCfg *cfg);
    /* 编码数据包
     * return: <0 error,otherwise the length of buffer
     */
    int32_t encode(lua_State *L, int32_t index, const char **buffer,
                   const CmdCfg *cfg);

private:
    bool _is_proto_loaded;
    class lprotobuf *_lprotobuf;
};
