#pragma once

#include "global/global.hpp"

class lprotobuf;
class ProtobufCodec final
{
public:
    ProtobufCodec();
    ~ProtobufCodec();

    void finalize();
    void reset();
    int32_t load(const char *buffer, size_t len);

    /* 解码数据包
     * return: <0 error,otherwise the number of parameter push to stack
     */
    int32_t decode(lua_State *L, const char *schema, const char *buffer, size_t len);
    /* 编码数据包
     * return: <0 error,otherwise the length of buffer
     */
    int32_t encode(lua_State *L, const char *schema, int32_t index,
                   const char **buffer);

private:
    bool _is_proto_loaded;
    class lprotobuf *_lprotobuf;
};
