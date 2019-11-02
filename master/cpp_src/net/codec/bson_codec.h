#pragma once

#include <lbson.h>
#include "codec.h"

// bson编码
class bson_codec : public codec
{
public:
    bson_codec();
    ~bson_codec();

    void finalize();
    int32_t load_path( const char *path ) { return 0; }

    /* 解码数据包
     * return: <0 error,otherwise the number of parameter push to stack
     */
    int32_t decode(
         lua_State *L,const char *buffer,int32_t len,const cmd_cfg_t *cfg );
    /* 编码数据包
     * return: <0 error,otherwise the length of buffer
     */
    int32_t encode(
        lua_State *L,int32_t index,const char **buffer,const cmd_cfg_t *cfg );
private:
    bson_t *_bson_doc;
};
