#include <websocket_parser.h>

#include "../socket.h"
#include "ws_stream_packet.h"
#include "../codec/codec_mgr.h"
#include "../../lua_cpplib/ltools.h"
#include "../../lua_cpplib/lstate.h"
#include "../../lua_cpplib/lnetwork_mgr.h"

// 发往客户端数据包头
struct clt_header
{
    uint16 _cmd;
    uint16 _errno; /* 错误码 */
};

// 发往服务器数据包头
struct srv_header
{
    uint16 _cmd;
};

ws_stream_packet::~ws_stream_packet()
{
}

ws_stream_packet::ws_stream_packet( class socket *sk ) : websocket_packet( sk )
{
}

/* 打包服务器发往客户端数据包
 * pack_clt( cmd,errno,flags,ctx )
 * return: <0 error;>=0 success
 */
int32 ws_stream_packet::pack_clt( lua_State *L,int32 index )
{
    if ( !_is_upgrade ) return http_packet::pack_clt( L,index );

    struct clt_header header;
    header._cmd = luaL_checkinteger( L,index );
    header._errno = luaL_checkinteger( L,index + 1 );

    websocket_flags flags = 
        static_cast<websocket_flags>( luaL_checkinteger( L,index + 2 ) );

    static const class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();
    const cmd_cfg_t *cfg = network_mgr->get_sc_cmd( header._cmd );
    if ( !cfg )
    {
        return luaL_error( L,"no command conf found: %d",header._cmd );
    }

    codec *encoder = 
        codec_mgr::instance()->get_codec( _socket->get_codec_type() );

    const char *ctx = NULL;
    int32 size = encoder->encode( L,index + 3,&ctx,cfg );
    if ( size < 0 ) return -1;

    if ( size > MAX_PACKET_LEN )
    {
        encoder->finalize();
        return luaL_error( L,"buffer size over MAX_PACKET_LEN" );
    }

    size_t frame_size = size + sizeof(header);
    class buffer &send = _socket->send_buffer();
    if ( !send.reserved( frame_size ) )
    {
        encoder->finalize();
        return luaL_error( L,"can not reserved buffer" );
    }

    const char *header_ctx = reinterpret_cast<const char*>(&header);
    size_t len = websocket_calc_frame_size( flags,frame_size );

    uint8 mask_offset = 0;
    char *buff = send.buff_pointer();
    static const char mask[4] = { 0 }; /* 服务器发往客户端并不需要mask */
    size_t offset = websocket_build_frame_header( buff,flags,mask,frame_size );
    offset += websocket_append_frame( 
        buff + offset,flags,mask,header_ctx,sizeof(header),&mask_offset );
    websocket_append_frame( buff + offset,flags,mask,ctx,size,&mask_offset );

    encoder->finalize();
    send.increase( len );
    _socket->pending_send();

    return 0;
}

/* 打包客户端发往服务器数据包
 * pack_srv( cmd,flags,ctx )
 * return: <0 error;>=0 success
 */
int32 ws_stream_packet::pack_srv( lua_State *L,int32 index )
{
    if ( !_is_upgrade ) return http_packet::pack_srv( L,index );

    struct srv_header header;
    header._cmd = luaL_checkinteger( L,index );
    websocket_flags flags = 
        static_cast<websocket_flags>( luaL_checkinteger( L,index + 1 ) );

    static const class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();
    const cmd_cfg_t *cfg = network_mgr->get_cs_cmd( header._cmd );
    if ( !cfg )
    {
        return luaL_error( L,"no command conf found: %d",header._cmd );
    }

    codec *encoder = 
        codec_mgr::instance()->get_codec( _socket->get_codec_type() );

    const char *ctx = NULL;
    int32 size = encoder->encode( L,index + 3,&ctx,cfg );
    if ( size < 0 ) return -1;

    if ( size > MAX_PACKET_LEN )
    {
        encoder->finalize();
        return luaL_error( L,"buffer size over MAX_PACKET_LEN" );
    }

    size_t frame_size = size + sizeof(header);
    class buffer &send = _socket->send_buffer();
    if ( !send.reserved( frame_size ) )
    {
        encoder->finalize();
        return luaL_error( L,"can not reserved buffer" );
    }

    const char *header_ctx = reinterpret_cast<const char*>(&header);
    size_t len = websocket_calc_frame_size( flags,frame_size );

    char mask[4] = { 0 };
    new_masking_key( mask );

    uint8 mask_offset = 0;
    char *buff = send.buff_pointer();
    size_t offset = websocket_build_frame_header( buff,flags,mask,frame_size );
    offset += websocket_append_frame( 
        buff + offset,flags,mask,header_ctx,sizeof(header),&mask_offset );
    websocket_append_frame( buff + offset,flags,mask,ctx,size,&mask_offset );

    encoder->finalize();
    send.increase( len );
    _socket->pending_send();

    return 0;
}

/* 数据帧完成 */
int32 ws_stream_packet::on_frame_end()
{
    socket::conn_t conn_ty = _socket->conn_type();
    /* 客户端到服务器的连接(CSCN)收到的是服务器发放客户端的数据包(sc_command) */
    if ( socket::CNT_CSCN == conn_ty )
    {
        return sc_command();
    }

    static const class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();

    /* 服务器收到的包，看要不要转发 */
    struct srv_header *header = 
        reinterpret_cast<struct srv_header *>( _body.data_pointer() );

    uint32_t size = _body.data_size() - sizeof( header );
    const char *ctx = reinterpret_cast<const char *>( header + 1 );
    if ( network_mgr->cs_dispatch( header->_cmd,_socket,ctx,size ) ) return 0;

    return cs_command( header->_cmd,ctx,size );
}

/* 回调server to client的数据包 */
int32 ws_stream_packet::sc_command()
{
    static lua_State *L = lstate::instance()->state();
    static const class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();

    assert( "lua stack dirty",0 == lua_gettop(L) );
    struct clt_header *header = 
        reinterpret_cast<struct clt_header *>( _body.data_pointer() );
    const cmd_cfg_t *cmd_cfg = network_mgr->get_sc_cmd( header->_cmd );
    if ( !cmd_cfg )
    {
        ERROR( "sc_command cmd(%d) no cmd cfg found",header->_cmd );
        return 0;
    }

    lua_pushcfunction( L,traceback );
    lua_getglobal    ( L,"command_new" );
    lua_pushinteger  ( L,_socket->conn_id() );
    lua_pushinteger  ( L,header->_cmd );
    lua_pushinteger  ( L,header->_errno );

    uint32_t size = _body.data_size() - sizeof( header );
    const char *ctx = reinterpret_cast<const char *>( header + 1 );
    codec *decoder = 
        codec_mgr::instance()->get_codec( _socket->get_codec_type() );
    int32 cnt = decoder->decode( L,ctx,size,cmd_cfg );
    if ( cnt < 0 )
    {
        lua_settop( L,0 );
        return 0;
    }

    if ( expect_false( LUA_OK != lua_pcall( L,3 + cnt,0,1 ) ) )
    {
        ERROR( "websocket stream sc_command:%s",lua_tostring( L,-1 ) );
    }

    lua_settop( L,0 ); /* remove traceback */

    return _socket->fd() < 0 ? -1 : 0;
}

/* 回调 client to server 的数据包 */
int32 ws_stream_packet::cs_command( int32 cmd,const char *ctx,size_t size )
{
    static lua_State *L = lstate::instance()->state();
    static const class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();

    assert( "lua stack dirty",0 == lua_gettop(L) );
    const cmd_cfg_t *cmd_cfg = network_mgr->get_cs_cmd( cmd );
    if ( !cmd_cfg )
    {
        ERROR( "cs_command cmd(%d) no cmd cfg found",cmd );
        return 0;
    }

    lua_pushcfunction( L,traceback );
    lua_getglobal    ( L,"command_new" );
    lua_pushinteger  ( L,_socket->conn_id() );
    lua_pushinteger  ( L,cmd );

    codec *decoder = 
        codec_mgr::instance()->get_codec( _socket->get_codec_type() );
    int32 cnt = decoder->decode( L,ctx,size,cmd_cfg );
    if ( cnt < 0 )
    {
        lua_settop( L,0 );
        return 0;
    }

    if ( expect_false( LUA_OK != lua_pcall( L,2 + cnt,0,1 ) ) )
    {
        ERROR( "websocket stream cs_command:%s",lua_tostring( L,-1 ) );
    }

    lua_settop( L,0 ); /* remove traceback */

    return _socket->fd() < 0 ? -1 : 0;
}

int32 ws_stream_packet::raw_pack_clt( 
    int32 cmd,uint16 ecode,const char *ctx,size_t size )
{
    struct clt_header header;
    header._cmd = cmd;
    header._errno = ecode;
    size_t frame_size = size + sizeof(header);
    class buffer &send = _socket->send_buffer();
    if ( !send.reserved( frame_size ) )
    {
        ERROR( "ws stream raw_pack_clt can not reserve memory" );
        return -1;
    }

    const char *header_ctx = reinterpret_cast<const char*>(&header);
    // TODO: 这个flags在通用的服务器交互中传不过来，暂时hard-code，后面如有需求，
    // 再多加一字段传过来
    websocket_flags flags = WS_OP_BINARY;
    size_t len = websocket_calc_frame_size( flags,frame_size );

    char mask[4] = { 0 };
    new_masking_key( mask );

    uint8 mask_offset = 0;
    char *buff = send.buff_pointer();
    size_t offset = websocket_build_frame_header( buff,flags,mask,frame_size );
    offset += websocket_append_frame( 
        buff + offset,flags,mask,header_ctx,sizeof(header),&mask_offset );
    websocket_append_frame( buff + offset,flags,mask,ctx,size,&mask_offset );
    send.increase( len );
    _socket->pending_send();

    return 0;
}
