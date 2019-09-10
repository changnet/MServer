#pragma once

#include "codec.h"

class lflatbuffers;
class flatbuffers_codec : public codec
{
public:
    flatbuffers_codec();
    ~flatbuffers_codec();

    void finalize();
    int32 load_path( const char *path );

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
    class lflatbuffers *_lflatbuffers;
};
