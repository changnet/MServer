#include "flatbuffers_codec.h"

#include <lflatbuffers.hpp>
#include "../net_include.h"
#include "../../global/assert.h"

flatbuffers_codec::flatbuffers_codec()
{
    _lflatbuffers = new class lflatbuffers();
}

flatbuffers_codec::~flatbuffers_codec()
{
    delete _lflatbuffers;
    _lflatbuffers = NULL;
}

void flatbuffers_codec::finalize()
{
}

/* 解码数据包
 * return: <0 error,otherwise the number of parameter push to stack
 */
int32 flatbuffers_codec::decode(
     lua_State *L,const char *buffer,int32 len,const cmd_cfg_t *cfg )
{
    if ( _lflatbuffers->decode( L,cfg->_schema,cfg->_object,buffer,len ) < 0 )
    {
        ERROR( "flatbuffers decode:%s",_lflatbuffers->last_error() );
        return -1;
    }

    // 默认情况下，decode把所有内容解析到一个table
    return 1;
}

/* 编码数据包
 * return: <0 error,otherwise the length of buffer
 */
int32 flatbuffers_codec::encode(
    lua_State *L,int32 index,const char **buffer,const cmd_cfg_t *cfg )
{
    if ( _lflatbuffers->encode( L,cfg->_schema,cfg->_object,index ) < 0 )
    {
        ERROR( "flatbuffers encode:%s",_lflatbuffers->last_error() );
        return -1;
    }

    size_t size = 0;
    *buffer = _lflatbuffers->get_buffer( size );

    return static_cast<int32>( size );
}
