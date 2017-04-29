#include "../lua_cpplib/ltools.h"
#include "../lua_cpplib/lstate.h"

/* 解析数据包
 * 返回push到栈上的参数数量
 */
int32 packet::do_parse( lua_State *L,const c2s_header *header )
{
    class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();
    const cmd_cfg_t *cmd_cfg = network_mgr->get_cmd_cfg( header->_cmd );
    if ( !cmd_cfg )
    {
        ERROR( "no command config found:%d",header->_cmd );
        return -1;
    }

    int32 size = PACKET_BUFFER_LEN( header );
    if ( size <= 0 || size > MAX_PACKET_LEN )
    {
        ERROR( "illegal packet buffer length" );
        return -1;
    }

    /* 去掉header内容 */
    const char *buffer = reinterpret_cast<const char *>( header + 1 );
    if ( _lflatbuffers.decode( 
        L,cmd_cfg->_schema,cmd_cfg->_object,buffer,size ) < 0 )
    {
        ERROR( "decode cmd(%d):%s",header->_cmd,_lflatbuffers.last_error() );
        return -1;
    }

    return 1;
}

void packet::do_parse( const stream_socket *sk,const s2c_header *header )
{

}

void packet::do_parse( const stream_socket *sk,const s2s_header *header )
{

}