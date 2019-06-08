#include <websocket_parser.h>

#include "../socket.h"
#include "ws_stream_packet.h"
#include "../../lua_cpplib/ltools.h"
#include "../../system/static_global.h"

#pragma pack (push, 1)

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

#pragma pack(pop)

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
    // 允许握手未完成就发数据，自己保证顺序
    // if ( !_is_upgrade ) return http_packet::pack_clt( L,index );

    int32 cmd    = luaL_checkinteger( L,index );
    uint16 ecode = luaL_checkinteger( L,index + 1 );

    websocket_flags flags = 
        static_cast<websocket_flags>( luaL_checkinteger( L,index + 2 ) );

    static const class lnetwork_mgr *network_mgr = static_global::network_mgr();
    const cmd_cfg_t *cfg = network_mgr->get_sc_cmd( cmd );
    if ( !cfg )
    {
        return luaL_error( L,"no command conf found: %d",cmd );
    }

    codec *encoder = 
        static_global::codec_mgr()->get_codec( _socket->get_codec_type() );

    const char *ctx = NULL;
    int32 size = encoder->encode( L,index + 3,&ctx,cfg );
    if ( size < 0 ) return -1;

    if ( size > MAX_PACKET_LEN )
    {
        encoder->finalize();
        return luaL_error( L,"buffer size over MAX_PACKET_LEN" );
    }

    if ( do_pack_clt( flags,cmd,ecode,ctx,size ) < 0 )
    {
        encoder->finalize();
        return luaL_error( L,"can not do_pack_clt" );
    }

    encoder->finalize();
    return 0;
}

/* 打包客户端发往服务器数据包
 * pack_srv( cmd,flags,ctx )
 * return: <0 error;>=0 success
 */
int32 ws_stream_packet::pack_srv( lua_State *L,int32 index )
{
    // 允许握手未完成就发数据，自己保证顺序
    // if ( !_is_upgrade ) return luaL_error( L,"websocket not upgrade" );

    struct srv_header header;
    header._cmd = luaL_checkinteger( L,index );
    websocket_flags flags = 
        static_cast<websocket_flags>( luaL_checkinteger( L,index + 1 ) );

    static const class lnetwork_mgr *network_mgr = static_global::network_mgr();
    const cmd_cfg_t *cfg = network_mgr->get_cs_cmd( header._cmd );
    if ( !cfg )
    {
        return luaL_error( L,"no command conf found: %d",header._cmd );
    }

    codec *encoder = 
        static_global::codec_mgr()->get_codec( _socket->get_codec_type() );

    const char *ctx = NULL;
    int32 size = encoder->encode( L,index + 2,&ctx,cfg );
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
    send.add_used_offset( len );
    _socket->pending_send();

    return 0;
}

/* 数据帧完成 */
int32 ws_stream_packet::on_frame_end()
{
    socket::conn_t conn_ty = _socket->conn_type();
    /* 客户端到服务器的连接(CSCN)收到的是服务器发往客户端的数据包(sc_command) */
    if ( socket::CNT_CSCN == conn_ty )
    {
        return sc_command();
    }

    static const class lnetwork_mgr *network_mgr = static_global::network_mgr();

    /* 服务器收到的包，看要不要转发 */
    uint32 data_size = 0;
    char *ctx = _body.check_all_used_ctx( data_size );
    if ( data_size < sizeof(struct srv_header) )
    {
        ERROR( "ws_stream_packet on_frame_end packet incomplete" );
        return 0;
    }
    struct srv_header *header = 
        reinterpret_cast<struct srv_header *>( ctx );

    uint32_t size = data_size - sizeof( *header );
    const char *ctx = reinterpret_cast<const char *>( header + 1 );
    if ( network_mgr->cs_dispatch( header->_cmd,_socket,ctx,size ) ) return 0;

    return cs_command( header->_cmd,ctx,size );
}

/* 回调server to client的数据包 */
int32 ws_stream_packet::sc_command()
{
    static lua_State *L = static_global::state();
    static const class lnetwork_mgr *network_mgr = static_global::network_mgr();

    assert( "lua stack dirty",0 == lua_gettop(L) );

    uint32 data_size = 0;
    char *ctx = _body.check_all_used_ctx( data_size );
    if ( data_size < sizeof(struct clt_header) )
    {
        ERROR( "ws_stream_packet sc_command packet incomplete" );
        return 0;
    }

    struct clt_header *header = 
        reinterpret_cast<struct clt_header *>( ctx );
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

    uint32_t size = data_size - sizeof( *header );
    const char *ctx = reinterpret_cast<const char *>( header + 1 );
    codec *decoder = 
        static_global::codec_mgr()->get_codec( _socket->get_codec_type() );
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
    static lua_State *L = static_global::state();
    static const class lnetwork_mgr *network_mgr = static_global::network_mgr();

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
        static_global::codec_mgr()->get_codec( _socket->get_codec_type() );
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
    // TODO: 这个flags在通用的服务器交互中传不过来，暂时hard-code.
    // 后面如有需求，再多加一字段传过来
    websocket_flags flags = 
        static_cast<websocket_flags>(WS_OP_BINARY | WS_FINAL_FRAME);

    return do_pack_clt( flags,cmd,ecode,ctx,size );
}

int32 ws_stream_packet::do_pack_clt(
    int32 raw_flags,int32 cmd,uint16 ecode,const char *ctx,size_t size )
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

    websocket_flags flags = static_cast<websocket_flags>( raw_flags );
    const char *header_ctx = reinterpret_cast<const char*>(&header);
    size_t len = websocket_calc_frame_size( flags,frame_size );

    char mask[4] = { 0 };
    uint8 mask_offset = 0;
    char *buff = send.buff_pointer();
    size_t offset = websocket_build_frame_header( buff,flags,mask,frame_size );
    offset += websocket_append_frame( 
        buff + offset,flags,mask,header_ctx,sizeof(header),&mask_offset );
    websocket_append_frame( buff + offset,flags,mask,ctx,size,&mask_offset );
    send.add_used_offset( len );
    _socket->pending_send();

    return 0;
}
