#include "packet.h"
#include "stream_socket.h"
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

class packet *packet::_packet = NULL;

void packet::uninstance()
{
    if ( _packet )
    {
        delete _packet;
    }
    _packet = NULL;
}

class packet *packet::instance()
{
    if ( !_packet )
    {
        _packet = new class packet();
    }

    return _packet;
}

/* 转客户端数据包 */
void packet::clt_forwarding( 
    const stream_socket *sk,const c2s_header *header,int32 session )
{
    class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();
    class socket *dest_sk  = network_mgr->get_connection( session );
    if ( !dest_sk )
    {
        ERROR( "client packet forwarding "
            "no destination found.cmd:%d",header->_cmd );
        return;
    }

    size_t size = PACKET_LENGTH( header );
    class buffer &send = dest_sk->send_buffer();
    if ( !send.reserved( size + sizeof(struct s2s_header) ) )
    {
        ERROR( "client packet forwarding,can not "
            "reserved memory:%ld",int64(size + sizeof(struct s2s_header)) );
        return;
    }

    struct s2s_header s2sh;
    s2sh._length = PACKET_MAKE_LENGTH( struct s2s_header,size );
    s2sh._cmd    = 0;
    s2sh._mask   = PKT_CSPK;
    s2sh._owner  = sk->get_owner();

    send.__append( &s2sh,sizeof(struct s2s_header) );
    send.__append( header,size );

    dest_sk->pending_send();
}

/* 解析客户端发往服务器的数据包 */
void packet::parse( const stream_socket *sk,const c2s_header *header )
{
    class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();
    const cmd_cfg_t *cmd_cfg = network_mgr->get_cmd_cfg( header->_cmd );
    if ( !cmd_cfg )
    {
        ERROR( "c2s cmd(%d) no cmd cfg found",header->_cmd );
        return;
    }

    /* 这个指令不是在当前进程处理，自动转发到对应进程 */
    if ( cmd_cfg->_session != network_mgr->session() )
    {
        clt_forwarding( sk,header,cmd_cfg->_session );
        return;
    }

    /* 在当前进程处理 */
    clt_command( sk,header );
}

void packet::parse( const stream_socket *sk,const s2c_header *header )
{
    //do_parse( buffer,header );
}

void packet::parse( const stream_socket *sk,const s2s_header *header )
{

}

int32 packet::load_schema( const char *path )
{
    return _lflatbuffers.load_bfbs_path( path );
}

/* 客户端数据包回调脚本 */
void packet::clt_command( const stream_socket *sk,const c2s_header *header )
{
    lua_State *L = lstate::instance()->state();
    assert( "lua stack dirty",0 == lua_gettop(L) );

    lua_pushcfunction( L,traceback );
    lua_getglobal( L,"socket_command" );
    lua_pushinteger( L,sk->conn_id() );
    lua_pushinteger( L,sk->get_owner() );

    int32 cnt = do_parse( L,header );
    if ( cnt < 0 )
    {
        lua_pop( L,4 );
        return;
    }

    if ( expect_false( LUA_OK != lua_pcall( L,4 + cnt,0,1 ) ) )
    {
        ERROR( "clt_command:%s",lua_tostring( L,-1 ) );

        lua_pop( L,1 ); /* remove traceback and error object */
        return;
    }
    lua_pop( L,1 ); /* remove traceback */
}