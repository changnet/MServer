#pragma once

#include "codec.h"

class lflatbuffers;
class FlatbuffersCodec : public Codec
{
public:
    FlatbuffersCodec();
    ~FlatbuffersCodec();

    void finalize();
    int32_t load_path( const char *path );

    /* 解码数据包
     * return: <0 error,otherwise the number of parameter push to stack
     */
    int32_t decode(
         lua_State *L,const char *buffer,int32_t len,const CmdCfg *cfg );
    /* 编码数据包
     * return: <0 error,otherwise the length of buffer
     */
    int32_t encode(
        lua_State *L,int32_t index,const char **buffer,const CmdCfg *cfg );
private:
    class lflatbuffers *_lflatbuffers;
};
