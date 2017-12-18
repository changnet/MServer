#include <websocket_parser.h>

#include "../socket.h"
#include "ws_stream_packet.h"
#include "../../lua_cpplib/ltools.h"
#include "../../lua_cpplib/lstate.h"

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

    size_t size = 0;
    const char *ctx = luaL_checklstring( L,index + 3,&size );
    if ( !ctx ) return 0;

    size_t frame_size = size + sizeof(header);
    class buffer &send = _socket->send_buffer();
    if ( !send.reserved( frame_size ) )
    {
        return luaL_error( L,"can not reserved buffer" );
    }

    size_t len = websocket_calc_frame_size( flags,frame_size );

    char *buff = send.buff_pointer();
    static const char mask[4] = { 0 }; /* 服务器发往客户端并不需要mask */
    size_t offset = websocket_build_frame( 
        buff,flags,mask,reinterpret_cast<const char*>(&header),sizeof(header) );
    websocket_append_frame( buff + offset,flags,mask,ctx,size );
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

    size_t size = 0;
    const char *ctx = luaL_checklstring( L,index + 2,&size );
    if ( !ctx ) return 0;

    size_t frame_size = size + sizeof(header);
    class buffer &send = _socket->send_buffer();
    if ( !send.reserved( frame_size ) )
    {
        return luaL_error( L,"can not reserved buffer" );
    }

    size_t len = websocket_calc_frame_size( flags,frame_size );

    char mask[4] = { 0 };
    new_masking_key( mask );

    char *buff = send.buff_pointer();
    size_t offset = websocket_build_frame( 
        buff,flags,mask,reinterpret_cast<const char*>(&header),sizeof(header) );
    websocket_append_frame( buff + offset,flags,mask,ctx,size );
    send.increase( len );
    _socket->pending_send();

    return 0;
}

/* 数据帧完成 */
int32 ws_stream_packet::on_frame_end()
{
    static lua_State *L = lstate::instance()->state();
    assert( "lua stack dirty",0 == lua_gettop(L) );

    uint32_t size = _body.data_size();
    const char *buff = _body.data_pointer();
    socket::conn_t conn_ty = _socket->conn_type();

    size_t offset = 0;

    lua_pushcfunction( L,traceback );
    lua_getglobal    ( L,"command_new" );
    lua_pushinteger  ( L,_socket->conn_id() );
    lua_pushlstring   ( L,buff + offset,size - offset );

    if ( expect_false( LUA_OK != lua_pcall( L,2,0,1 ) ) )
    {
        ERROR( "websocket command:%s",lua_tostring( L,-1 ) );
    }

    lua_settop( L,0 ); /* remove traceback */

    return _socket->fd() < 0 ? -1 : 0;
}
