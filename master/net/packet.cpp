#include "packet.h"
#include "../lua_cpplib/lnetwork_mgr.h"

#if defined FLATBUFFERS_PARSE
    #include "packet_flatbuffers.cpp"
#elif defined PROTOBUF_PARSE
    #include "packet_protobuf.cpp"
#elif defined STREAM_PARSE
    #include "packet_stream.cpp"
#else
    #error no header parse specify
#endif

void packet::forwarding( 
    packet_t pkt,const char *buffer,size_t size,int32 session )
{
    struct s2s_header s2sh;
    s2sh._length = PACKET_MAKE_LENGTH( struct s2s_header,size );
    s2sh._cmd    = static_cast<uint16>  ( srv_cmd );
    s2sh._pid    = pid;

    _send.__append( &s2sh,sizeof(struct s2s_header) );
    _send.__append( clt_recv.data(),len );
}

/* 解析客户端发往服务器的数据包 */
void packet::parse_header( const char *buffer,const c2s_header *header )
{
    class lnetwork_mgr *network_mgr = lnetwork_mgr:instance();
    const cmd_cfg_t *cmd_cfg = network_mgr->get_cmd_cfg( header->_cmd );
    if ( !cmd_cfg )
    {
        ERROR( "c2s cmd(%d) no cmd cfg found",header->_cmd );
        return;
    }

    /* 这个指令不是在当前进程处理，自动转发到对应进程 */
    if ( cmd_cfg->_session != network_mgr->session() )
    {
        forwarding( PKT_CSPK,buffer,PACKET_LENGTH( header ),cmd_cfg->_session );
        return;
    }
}

void packet::parse_header( const char *buffer,const s2c_header *header )
{
    do_parse_header( buffer,header );
}

void packet::parse_header( const char *buffer,const s2s_header *header )
{

}
