#include "../lua_cpplib/ltools.h"
#include "../lua_cpplib/lstate.h"

/* 加载该目录下的有schema文件 */
int32 packet::load_schema( const char *path )
{
    return _lflatbuffers.load_bfbs_path( path );
}

/* 解析数据包
 * 返回push到栈上的参数数量
 */
int32 packet::parse( lua_State *L,
    const char *schema,const char *object,const c2s_header *header )
{
    int32 size = PACKET_BUFFER_LEN( header );
    if ( size <= 0 || size > MAX_PACKET_LEN )
    {
        ERROR( "illegal packet buffer length" );
        return -1;
    }

    /* 去掉header内容 */
    const char *buffer = reinterpret_cast<const char *>( header + 1 );
    if ( _lflatbuffers.decode( L,schema,object,buffer,size ) < 0 )
    {
        ERROR( "decode cmd(%d):%s",header->_cmd,_lflatbuffers.last_error() );
        return -1;
    }

    return 1;
}

