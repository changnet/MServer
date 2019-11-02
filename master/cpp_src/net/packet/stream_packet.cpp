#include "stream_packet.h"

#include "../socket.h"
#include "../../lua_cpplib/ltools.h"
#include "../../system/static_global.h"

StreamPacket::StreamPacket( class Socket *sk )
    : Packet( sk )
{
}

StreamPacket::~StreamPacket()
{
}

/* 解析二进制流数据包 */
int32_t StreamPacket::unpack()
{
    class Buffer &recv = _socket->recv_buffer();

    // 检测包头是否完整
    if ( !recv.check_used_size( sizeof( struct base_header ) ) ) return 0;

    const struct base_header *header =
        reinterpret_cast<const struct base_header *>(
            recv.to_continuous_ctx( sizeof( struct base_header ) ) );

    // 检测包内容是否完整
    uint32_t length = header->_length;
    if ( !recv.check_used_size( length ) ) return 0;

    header = reinterpret_cast<
        const struct base_header *>(recv.to_continuous_ctx( length ) );

    dispatch( header ); // 数据包完整，派发处理
    recv.remove( length );   // 无论成功或失败，都移除该数据包

    return length;
}

void StreamPacket::dispatch( const struct base_header *header )
{
    switch( _socket->conn_type() )
    {
        case Socket::CT_CSCN: /* 解析服务器发往客户端的包 */
            sc_command( reinterpret_cast<const struct s2c_header *>(header) );
            break;
        case Socket::CT_SCCN: /* 解析客户端发往服务器的包 */
            cs_dispatch( reinterpret_cast<const struct c2s_header *>(header) );
            break;
        case Socket::CT_SSCN: /* 解析服务器发往服务器的包 */
            process_ss_command(
                reinterpret_cast<const struct s2s_header *>( header ) );
            break;
        default :
            ERROR("stream_packet dispatch "
                "unknow connect type:%d",_socket->conn_type());
            return;
    }
}

/* 服务器发往客户端数据包回调脚本 */
void StreamPacket::sc_command( const struct s2c_header *header )
{
    static lua_State *L = StaticGlobal::state();
    static const class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

    ASSERT( 0 == lua_gettop(L), "lua stack dirty" );
    const CmdCfg *cmd_cfg = network_mgr->get_sc_cmd( header->_cmd );
    if ( !cmd_cfg )
    {
        ERROR( "sc_command cmd(%d) cfg not found",header->_cmd );
        return;
    }

    int32_t size = PACKET_BUFFER_LEN( header );
    /* 去掉header内容 */
    const char *buffer = reinterpret_cast<const char *>( header + 1 );

    uint32_t conn_id = _socket->conn_id();

    lua_pushcfunction( L,traceback );
    lua_getglobal( L,"command_new" );
    lua_pushinteger( L,conn_id );
    lua_pushinteger( L,header->_cmd );
    lua_pushinteger( L,header->_errno );

    class Codec *decoder =
        StaticGlobal::codec_mgr()->get_codec( _socket->get_codec_type() );
    if ( !decoder )
    {
        ERROR( "command_new no codec conf found: %d",header->_cmd );
        lua_settop( L,0 );
        return;
    }
    int32_t cnt = decoder->decode( L,buffer,size,cmd_cfg );
    if ( cnt < 0 )
    {
        lua_settop( L,0 );
        return;
    }

    if ( EXPECT_FALSE( LUA_OK != lua_pcall( L,3 + cnt,0,1 ) ) )
    {
        ERROR( "sc_command:%s",lua_tostring( L,-1 ) );

        lua_settop( L,0 ); /* remove traceback and error object */
        return;
    }
    lua_settop( L,0 ); /* remove traceback */
}

/* 派发客户端发给服务器数据包 */
void StreamPacket::cs_dispatch( const struct c2s_header *header )
{
    static const class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

    int32_t cmd = header->_cmd;
    int32_t size = PACKET_BUFFER_LEN( header );
    const char *ctx = reinterpret_cast<const char *>( header + 1 );

    /* 这个指令不是在当前进程处理，自动转发到对应进程 */
    if ( network_mgr->cs_dispatch( cmd,_socket,ctx,size ) ) return;

    cs_command( cmd,ctx,size);/* 在当前进程处理 */
}

/* 客户端发往服务器数据包回调脚本 */
void StreamPacket::cs_command( int32_t cmd,const char *ctx,size_t size )
{
    static lua_State *L = StaticGlobal::state();
    static const class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

    ASSERT( 0 == lua_gettop(L),"lua stack dirty" );

    const CmdCfg *cmd_cfg = network_mgr->get_cs_cmd( cmd );
    if ( !cmd_cfg )
    {
        ERROR( "cs_command cmd(%d) no cmd cfg found",cmd );
        return;
    }

    int32_t conn_id = _socket->conn_id();
    Codec::CodecType codec_ty = _socket->get_codec_type();

    lua_pushcfunction( L,traceback );
    lua_getglobal    ( L,"command_new" );
    lua_pushinteger  ( L,conn_id );
    lua_pushinteger  ( L,cmd     );

    Codec *decoder = StaticGlobal::codec_mgr()->get_codec( codec_ty );
    if ( !decoder )
    {
        ERROR( "command_new no codec found:%d",cmd );
        lua_settop( L,0 );
        return;
    }
    int32_t cnt = decoder->decode( L,ctx,size,cmd_cfg );
    if ( cnt < 0 )
    {
        lua_settop( L,0 );
        return;
    }

    if ( EXPECT_FALSE( LUA_OK != lua_pcall( L,2 + cnt,0,1 ) ) )
    {
        ERROR( "cs_command:%s",lua_tostring( L,-1 ) );

        lua_settop( L,0 ); /* remove traceback and error object */
        return;
    }
    lua_settop( L,0 ); /* remove traceback */
}


/* 处理服务器之间数据包 */
void StreamPacket::process_ss_command( const s2s_header *header )
{
    /* 先判断数据包类型 */
    switch ( header->_packet )
    {
        case SPT_SSPK : ss_dispatch  ( header );return;
        case SPT_CSPK : css_command  ( header );return;
        case SPT_SCPK : ssc_command  ( header );return;
        case SPT_RPCS : rpc_command  ( header );return;
        case SPT_RPCR : rpc_return   ( header );return;
        case SPT_CBCP : ssc_multicast( header );return;
        default :
        {
            ERROR( "unknow server "
                "packet:cmd %d,packet type %d",header->_cmd,header->_packet );
            return;
        }
    }
}

/* 派发服务器之间的数据包 */
void StreamPacket::ss_dispatch( const s2s_header *header )
{
    static const class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

    const CmdCfg *cmd_cfg = network_mgr->get_ss_cmd( header->_cmd );
    if ( !cmd_cfg )
    {
        ERROR( "ss_dispatch cmd(%d) no cmd cfg found",header->_cmd );
        return;
    }

    /* 这个指令不是在当前进程处理，自动转发到对应进程 */
    // 暂时停用服务器数据包自动转发 !!! 2019-01-01
    // 在实际使用中，这个功能较为少用。但是会引起一些麻烦。
    // 比如错误地广播一个数据包，会导致其他进程把数据包转发给一个进程。
    // if ( cmd_cfg->_session != network_mgr->get_curr_session() )
    // {
    //     class socket *dest_sk  =
    //         network_mgr->get_conn_by_session( cmd_cfg->_session );
    //     if ( !dest_sk )
    //     {
    //         ERROR( "server packet forwarding "
    //             "no destination found.cmd:%d",header->_cmd );
    //         return;
    //     }

    //     bool is_ok = dest_sk->append( header,PACKET_LENGTH( header ) );
    //     if ( !is_ok )
    //     {
    //         ERROR( "server packet forwrding "
    //             "can not reserved memory:" FMT64d,int64(PACKET_LENGTH( header )) );
    //     }
    //     return;
    // }

    ss_command( header,cmd_cfg );
}

/* 服务器发往服务器数据包回调脚本 */
void StreamPacket::ss_command(
    const s2s_header *header,const CmdCfg *cmd_cfg )
{
    static lua_State *L = StaticGlobal::state();
    ASSERT( 0 == lua_gettop(L),"lua stack dirty" );

    int32_t size = PACKET_BUFFER_LEN( header );
    /* 去掉header内容 */
    const char *buffer = reinterpret_cast<const char *>( header + 1 );

    lua_pushcfunction( L,traceback );
    lua_getglobal( L,"command_new" );
    lua_pushinteger( L,_socket->conn_id() );
    lua_pushinteger( L,header->_owner );
    lua_pushinteger( L,header->_cmd );
    lua_pushinteger( L,header->_errno );

    Codec *decoder =
        StaticGlobal::codec_mgr()->get_codec( _socket->get_codec_type() );
    if ( !decoder )
    {
        ERROR( "ss command_new no codec found:%d",header->_cmd );
        lua_settop( L,0 );
        return;
    }
    int32_t cnt = decoder->decode( L,buffer,size,cmd_cfg );
    if ( cnt < 0 )
    {
        lua_settop( L,0 );
        return;
    }

    if ( EXPECT_FALSE( LUA_OK != lua_pcall( L,4 + cnt,0,1 ) ) )
    {
        ERROR( "ss_command:%s",lua_tostring( L,-1 ) );

        lua_settop( L,0 ); /* remove traceback and error object */
        return;
    }
    lua_settop( L,0 ); /* remove traceback */
}

/* 客户端发往服务器，由网关转发的数据包回调脚本 */
void StreamPacket::css_command( const s2s_header *header )
{
    static lua_State *L = StaticGlobal::state();
    static const class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

    ASSERT( 0 == lua_gettop(L),"lua stack dirty" );

    const CmdCfg *cmd_cfg = network_mgr->get_cs_cmd( header->_cmd );
    if ( !cmd_cfg )
    {
        ERROR( "css_command cmd(%d) no cmd cfg found",header->_cmd );
        return;
    }

    int32_t size = PACKET_BUFFER_LEN( header );
    /* 去掉header内容 */
    const char *buffer = reinterpret_cast<const char *>( header + 1 );

    lua_pushcfunction( L,traceback );
    lua_getglobal    ( L,"css_command_new" );
    lua_pushinteger  ( L,_socket->conn_id() );
    lua_pushinteger  ( L,header->_owner   );
    lua_pushinteger  ( L,header->_cmd );

    Codec *decoder = StaticGlobal::codec_mgr()
        ->get_codec( static_cast<Codec::CodecType>(header->_codec) );
    if ( !decoder )
    {
        ERROR( "css_command_new no codec found:%d",header->_cmd );
        lua_settop( L,0 );
        return;
    }
    int32_t cnt = decoder->decode( L,buffer,size,cmd_cfg );
    if ( cnt < 0 )
    {
        lua_settop( L,0 );
        return;
    }

    if ( EXPECT_FALSE( LUA_OK != lua_pcall( L,3 + cnt,0,1 ) ) )
    {
        ERROR( "css_command:%s",lua_tostring( L,-1 ) );

        lua_settop( L,0 ); /* remove traceback and error object */
        return;
    }
    lua_settop( L,0 ); /* remove traceback */
}


/* 解析其他服务器转发到网关的客户端数据包 */
void StreamPacket::ssc_command( const s2s_header *header )
{
    static const class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

    class Socket *sk = network_mgr->get_conn_by_owner( header->_owner );
    if ( !sk )
    {
        ERROR( "ssc packet no clt connect found" );
        return;
    }

    if ( Socket::CT_SCCN != sk->conn_type() )
    {
        ERROR( "ssc packet destination conn is not a clt" );
        return;
    }

    int32_t size = PACKET_BUFFER_LEN( header );
    const char *ctx = reinterpret_cast<const char *>( header + 1 );
    class Packet *sk_packet = sk->get_packet();
    sk_packet->raw_pack_clt( header->_cmd,header->_errno,ctx,size );
}


/* 处理rpc调用 */
void StreamPacket::rpc_command( const s2s_header *header )
{
    static lua_State *L = StaticGlobal::state();
    ASSERT( 0 == lua_gettop(L),"lua stack dirty" );

    int32_t size = PACKET_BUFFER_LEN( header );
    /* 去掉header内容 */
    const char *buffer = reinterpret_cast<const char *>( header + 1 );

    lua_pushcfunction( L,traceback );
    int32_t top = lua_gettop( L ); // pcall后，下面的栈都会被弹出

    lua_getglobal    ( L,"rpc_command_new"  );
    lua_pushinteger  ( L,_socket->conn_id() );
    lua_pushinteger  ( L,header->_owner     );

    // rpc解析方式目前固定为bson
    Codec *decoder = StaticGlobal::codec_mgr()->get_codec( Codec::CDC_BSON );
    int32_t cnt = decoder->decode( L,buffer,size,NULL );
    if ( cnt < 1 ) // rpc调用至少要带参数名
    {
        lua_settop( L,0 );
        ERROR( "rpc command miss function name" );
        return;
    }

    int32_t unique_id = static_cast<int32_t>( header->_owner );
    int32_t ecode = lua_pcall( L,2 + cnt,LUA_MULTRET,1 );
    // unique_id是rpc调用的唯一标识，如果不为0，则需要返回结果
    if ( unique_id > 0 )
    {
        do_pack_rpc( L,unique_id,ecode,SPT_RPCR,top + 1 );
    }

    if ( LUA_OK != ecode )
    {
        ERROR( "rpc command:%s",lua_tostring(L,-1) );
        lua_settop( L,0 ); /* remove error object and traceback */
        return;
    }

    lua_settop( L,0 ); /* remove trace back */
}

/* 处理rpc返回 */
void StreamPacket::rpc_return( const s2s_header *header )
{
    static lua_State *L = StaticGlobal::state();
    ASSERT( 0 == lua_gettop(L),"lua stack dirty" );

    int32_t size = PACKET_BUFFER_LEN( header );
    const char *buffer = reinterpret_cast<const char *>( header + 1 );

    lua_pushcfunction( L,traceback );
    lua_getglobal( L,"rpc_command_return" );
    lua_pushinteger( L,_socket->conn_id() );
    lua_pushinteger( L,header->_owner );
    lua_pushinteger( L,header->_errno );

    // rpc在出错的情况下仍返回，这时buff可能无法成功打包
    int32_t cnt = 0;
    if ( size > 0 )
    {
        // rpc解析方式目前固定为bson
        Codec *decoder = StaticGlobal::codec_mgr()->get_codec( Codec::CDC_BSON );
        cnt = decoder->decode( L,buffer,size,NULL );
    }
    if ( LUA_OK != lua_pcall( L,3 + cnt,0,1 ) )
    {
        ERROR( "rpc_return:%s",lua_tostring( L,-1 ) );

        lua_settop( L,0 ); /* remove traceback and error object */
        return;
    }
    lua_settop( L,0 ); /* remove traceback */

    return;
}

/* 打包rpc数据包 */
int32_t StreamPacket::do_pack_rpc(
    lua_State *L,int32_t unique_id,int32_t ecode,uint16_t pkt,int32_t index )
{
    STAT_TIME_BEG();

    int32_t len = 0;
    const char *buffer = NULL;
    Codec *encoder = StaticGlobal::codec_mgr()->get_codec( Codec::CDC_BSON );

    if ( LUA_OK == ecode )
    {
        len = encoder->encode( L,index,&buffer,NULL );
        if ( len < 0 )
        {
            // 发送时，出错就不发了
            // 返回时，出错也应该告知另一进程出错了
            if ( SPT_RPCS == pkt ) return -1;
            len = 0;
            ecode = -1;
        }
    }

    struct s2s_header s2sh;
    SET_HEADER_LENGTH( s2sh, len, 0, SET_LENGTH_FAIL_RETURN );
    s2sh._cmd    = 0;
    s2sh._errno  = ecode;
    s2sh._packet = pkt;
    s2sh._codec  = Codec::CDC_NONE; // 用不着，但不初始化valgrind会警告
    s2sh._owner  = unique_id;

    class Buffer &send = _socket->send_buffer();
    send.append( &s2sh,sizeof(struct s2s_header) );
    if ( len > 0)
    {
        send.append( buffer,static_cast<uint32_t>(len) );
    }
    _socket->pending_send();

    encoder->finalize();

    // rcp返回结果也是走这里，但是返回是不包含rpc函数名的
    if ( SPT_RPCS == pkt && lua_isstring(L,index) )
    {
        RPC_STAT_ADD( 
            lua_tostring(L,index),(int32_t)s2sh._length, STAT_TIME_END() );
    }

    return 0;
}

/* 打包服务器发往客户端数据包 */
int32_t StreamPacket::pack_clt( lua_State *L,int32_t index )
{
    STAT_TIME_BEG();
    static const class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

    int32_t cmd = luaL_checkinteger( L,index );
    int32_t ecode = luaL_checkinteger( L,index + 1 );

    const CmdCfg *cfg = network_mgr->get_sc_cmd( cmd );
    if ( !cfg )
    {
        return luaL_error( L,"no command conf found: %d",cmd );
    }

    Codec *encoder =
        StaticGlobal::codec_mgr()->get_codec( _socket->get_codec_type() );
    if ( !encoder )
    {
        return luaL_error( L,"no codec conf found: %d",cmd );
    }

    const char *buffer = NULL;
    int32_t len = encoder->encode( L,index + 2,&buffer,cfg );
    if ( len < 0 ) return -1;

    if (len > MAX_PACKET_LEN )
    {
        encoder->finalize();
        return luaL_error( L,"buffer size over MAX_PACKET_LEN" );
    }

    if ( raw_pack_clt( cmd,ecode,buffer,len ) < 0 )
    {
        encoder->finalize();
        return luaL_error( L,"can not raw pack clt" );
    }

    encoder->finalize();

    PKT_STAT_ADD( SPT_SCPK, 
        cmd, int32_t(len + sizeof(struct s2c_header)),STAT_TIME_END() );
    return 0;
}

/* 打包客户端发往服务器数据包 */
int32_t StreamPacket::pack_srv( lua_State *L,int32_t index )
{
    STAT_TIME_BEG();

    static const class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

    int32_t cmd = luaL_checkinteger( L,index );

    if ( !lua_istable( L,index + 1 ) )
    {
        return luaL_error( L,
            "expect table,got %s",lua_typename( L,lua_type(L,index + 1) ) );
    }

    const CmdCfg *cfg = network_mgr->get_cs_cmd( cmd );
    if ( !cfg )
    {
        return luaL_error( L,"no command conf found: %d",cmd );
    }

    Codec *encoder =
        StaticGlobal::codec_mgr()->get_codec( _socket->get_codec_type() );
    if ( !encoder )
    {
        return luaL_error( L,"no codec conf found: %d",cmd );
    }

    const char *buffer = NULL;
    int32_t len = encoder->encode( L,index + 1,&buffer,cfg );
    if ( len < 0 ) return -1;

    struct c2s_header c2sh;
    SET_HEADER_LENGTH( c2sh, len, cmd, SET_LENGTH_FAIL_ENCODE );
    c2sh._cmd    = static_cast<uint16_t>  ( cmd );

    class Buffer &send = _socket->send_buffer();
    send.append( &c2sh,sizeof(c2sh) );
    if (len > 0) send.append( buffer,len );

    encoder->finalize();
    _socket->pending_send();

    PKT_STAT_ADD( SPT_CSPK, cmd, int32_t(c2sh._length),STAT_TIME_END() );

    return 0;
}

int32_t StreamPacket::pack_ss ( lua_State *L,int32_t index )
{
    STAT_TIME_BEG();
    static const class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

    int32_t cmd = luaL_checkinteger( L,index );
    int32_t ecode = luaL_checkinteger( L,index + 1 );

    if ( !lua_istable( L,index + 2 ) )
    {
        return luaL_error( L,
            "expect table,got %s",lua_typename( L,lua_type(L,index + 2) ) );
    }

    const CmdCfg *cfg = network_mgr->get_ss_cmd( cmd );
    if ( !cfg )
    {
        return luaL_error( L,"no command conf found: %d",cmd );
    }

    Codec *encoder =
        StaticGlobal::codec_mgr()->get_codec( _socket->get_codec_type() );
    if ( !encoder )
    {
        return luaL_error( L,"no codec found: %d",cmd );
    }

    const char *buffer = NULL;
    int32_t len = encoder->encode( L,index + 2,&buffer,cfg );
    if ( len < 0 ) return -1;

    if ( len > MAX_PACKET_LEN )
    {
        encoder->finalize();
        return luaL_error( L,"buffer size over MAX_PACKET_LEN" );
    }

    int32_t session = network_mgr->get_curr_session();
    if ( raw_pack_ss( cmd,ecode,session,buffer,len ) < 0 )
    {
        encoder->finalize();
        return luaL_error( L,"can not raw_pack_ss" );
    }

    encoder->finalize();

    PKT_STAT_ADD( SPT_SSPK, 
        cmd, int32_t(len + sizeof(struct s2s_header)),STAT_TIME_END() );
    return 0;
}

int32_t StreamPacket::pack_rpc( lua_State *L,int32_t index )
{
    int32_t unique_id = luaL_checkinteger( L,index );
    // ecode默认0
    return do_pack_rpc( L,unique_id,0,SPT_RPCS,index + 1 );
}

int32_t StreamPacket::pack_ssc( lua_State *L,int32_t index )
{
    STAT_TIME_BEG();

    static const class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

    Owner owner  = luaL_checkinteger( L,index     );
    int32_t codec_ty = luaL_checkinteger( L,index + 1 );
    int32_t cmd      = luaL_checkinteger( L,index + 2 );
    int32_t ecode    = luaL_checkinteger( L,index + 3 );

    if ( codec_ty < Codec::CDC_NONE || codec_ty >= Codec::CDC_MAX )
    {
        return luaL_error( L,"illegal codec type" );
    }

    if ( !lua_istable( L,index + 4 ) )
    {
        return luaL_error( L,
            "expect table,got %s",lua_typename( L,lua_type(L,index + 4) ) );
    }

    const CmdCfg *cfg = network_mgr->get_sc_cmd( cmd );
    if ( !cfg )
    {
        return luaL_error( L,"no command conf found: %d",cmd );
    }

    Codec *encoder = StaticGlobal::codec_mgr()
        ->get_codec( static_cast<Codec::CodecType>(codec_ty) );
    if ( !encoder )
    {
        return luaL_error( L,"no command conf found: %d",cmd );
    }

    const char *buffer = NULL;
    int32_t len = encoder->encode( L,index + 4,&buffer,cfg );
    if ( len < 0 ) return -1;

    /* 把客户端数据包放到服务器数据包 */
    struct s2s_header s2sh;
    SET_HEADER_LENGTH( s2sh, len, cmd, SET_LENGTH_FAIL_ENCODE );
    s2sh._cmd    = static_cast<uint16_t>  ( cmd );
    s2sh._errno  = ecode;
    s2sh._owner  = owner;
    s2sh._codec  = Codec::CDC_NONE; /* 避免valgrind警告内存未初始化 */
    s2sh._packet = SPT_SCPK; /*指定数据包类型为服务器发送客户端 */

    class Buffer &send = _socket->send_buffer();
    send.append( &s2sh ,sizeof(s2sh) );
    if ( len > 0 ) send.append( buffer,len );

    encoder->finalize();
    _socket->pending_send();

    PKT_STAT_ADD( SPT_SSPK, cmd, int32_t(s2sh._length),STAT_TIME_END() );

    return 0;
}

int32_t StreamPacket::raw_pack_clt(
    int32_t cmd,uint16_t ecode,const char *ctx,size_t size )
{
    /* 先构造客户端收到的数据包 */
    struct s2c_header s2ch;
    SET_HEADER_LENGTH( s2ch, size, cmd, SET_LENGTH_FAIL_RETURN );
    s2ch._cmd    = static_cast<uint16_t>  ( cmd );
    s2ch._errno  = ecode;

    class Buffer &send = _socket->send_buffer();
    send.append( &s2ch ,sizeof(s2ch) );
    if ( size > 0 ) send.append( ctx,size );

    _socket->pending_send();
    return 0;
}

int32_t StreamPacket::raw_pack_ss(
    int32_t cmd,uint16_t ecode,int32_t session,const char *ctx,size_t size )
{
    struct s2s_header s2sh;
    SET_HEADER_LENGTH( s2sh, size, cmd, SET_LENGTH_FAIL_RETURN );
    s2sh._cmd    = static_cast<uint16_t> ( cmd );
    s2sh._errno  = ecode;
    s2sh._owner  = session;
    s2sh._packet = SPT_SSPK;
    s2sh._codec  = Codec::CDC_NONE;// 这个这里用不着，但不初始化valgrind就会警告

    class Buffer &send = _socket->send_buffer();
    send.append( &s2sh ,sizeof(s2sh) );
    if ( size > 0 ) send.append( ctx,size );

    _socket->pending_send();
    return 0;
}

// 打包客户端广播数据
// ssc_multicast( conn_id,mask,args_list,codec_type,cmd,errno,pkt )
int32_t StreamPacket::pack_ssc_multicast( lua_State *L,int32_t index )
{
    STAT_TIME_BEG();

    static const class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

    Owner list[MAX_CLT_CAST] = { 0 };
    int32_t mask     = luaL_checkinteger( L,index     );
    int32_t codec_ty = luaL_checkinteger( L,index + 2 );
    int32_t cmd      = luaL_checkinteger( L,index + 3 );
    int32_t ecode    = luaL_checkinteger( L,index + 4 );

    lUAL_CHECKTABLE( L,index + 1 );
    lUAL_CHECKTABLE( L,index + 5 );

    // 占用list的两个位置，这样写入socket缓存区时不用另外处理
    list[0] = mask;
    // list[1] = 0; 数量
    int32_t idx = 2;
    lua_pushnil(L);  /* first key */
    while ( lua_next(L, index + 1) != 0 )
    {
        if ( !lua_isinteger( L,-1 ) )
        {
            lua_pop( L, 1 );
            return luaL_error( L,"ssc_multicast list expect integer" );
        }

        if ( idx >= MAX_CLT_CAST )
        {
            ERROR( "pack_ssc_multicast too many id in list:%d",idx );
            lua_pop( L, 2 );
            break;
        }
        // 以后可能定义owner_t为unsigned类型，当传参数而不是owner时，不要传负数
        Owner owner = static_cast<Owner>( lua_tointeger(L,-1) );
        list[idx++] = owner;
        lua_pop( L, 1 );
    }

    list[1] = idx - 2; // 数量
    size_t list_len = sizeof(Owner)*idx;

    if ( codec_ty < Codec::CDC_NONE || codec_ty >= Codec::CDC_MAX )
    {
        return luaL_error( L,"illegal codec type" );
    }

    const CmdCfg *cfg = network_mgr->get_sc_cmd( cmd );
    if ( !cfg )
    {
        return luaL_error( L,"no command conf found: %d",cmd );
    }

    Codec *encoder = StaticGlobal::codec_mgr()
        ->get_codec( static_cast<Codec::CodecType>(codec_ty) );
    if ( !encoder )
    {
        return luaL_error( L,"no command conf found: %d",cmd );
    }

    const char *buffer = NULL;
    int32_t len = encoder->encode( L,index + 5,&buffer,cfg );
    if ( len < 0 ) return -1;

    /* 把客户端数据包放到服务器数据包 */
    struct s2s_header s2sh;
    SET_HEADER_LENGTH( s2sh, list_len + len, cmd, SET_LENGTH_FAIL_ENCODE );
    s2sh._cmd    = static_cast<uint16_t>  ( cmd );
    s2sh._errno  = ecode;
    s2sh._owner  = 0;
    s2sh._codec  = Codec::CDC_NONE; /* 避免valgrind警告内存未初始化 */
    s2sh._packet = SPT_CBCP; /*指定数据包类型为服务器发送客户端 */

    class Buffer &send = _socket->send_buffer();
    send.append( &s2sh ,sizeof(s2sh) );
    send.append( list,list_len );
    if ( len > 0 ) send.append( buffer,len );

    encoder->finalize();
    _socket->pending_send();

    PKT_STAT_ADD( SPT_CBCP, cmd, int32_t(s2sh._length),STAT_TIME_END() );

    return 0;
}

// 转发到一个客户端
void StreamPacket::ssc_one_multicast(
    Owner owner,int32_t cmd,uint16_t ecode,const char *ctx,int32_t size )
{
    static const class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

    class Socket *sk = network_mgr->get_conn_by_owner( owner );
    if ( !sk )
    {
        ERROR( "ssc_one_multicast no clt connect found" );
        return;
    }
    if ( Socket::CT_SCCN != sk->conn_type() )
    {
        ERROR( "ssc_one_multicast destination conn is not a clt" );
        return;
    }
    class Packet *sk_packet = sk->get_packet();
    if ( !sk_packet )
    {
        ERROR( "ssc_one_multicast no packet found" );
        return;
    }
    sk_packet->raw_pack_clt( cmd,ecode,ctx,size );
}

// 处理其他进程发过来的客户端广播
void StreamPacket::ssc_multicast( const s2s_header *header )
{
    const Owner *raw_list = reinterpret_cast<const Owner *>( header + 1 );
    int32_t mask = static_cast<int32_t>( *raw_list );
    int32_t count = static_cast<int32_t>( *(raw_list + 1) );
    if ( MAX_CLT_CAST < count )
    {
        ERROR( "ssc_multicast too many to cast:%d",count );
        return;
    }

    // 长度记得包含mask和count本身这两个变量
    size_t raw_list_len = sizeof(Owner)*(count + 2);
    int32_t size = PACKET_BUFFER_LEN( header ) - raw_list_len;
    const char *ctx =
        reinterpret_cast<const char *>( header + 1 );
    ctx += raw_list_len;

    if ( size < 0 )
    {
        ERROR( "ssc_multicast packet length broken" );
        return;
    }

    // 根据玩家pid广播，底层直接处理
    if ( CLT_MC_OWNER == mask )
    {
        for ( int32_t idx = 0;idx < count;idx ++ )
        {
            ssc_one_multicast(
                *(raw_list + idx + 2),header->_cmd,header->_errno,ctx,size );
        }
        return;
    }

    // 如果不是根据pid广播，那么这个list就是自定义参数，参数不能太多
    if ( count > 16 ) // 限制一下参数，防止lua栈溢出
    {
        ERROR( "ssc_multicast too many argument" );
        return;
    }

    static lua_State *L = StaticGlobal::state();
    ASSERT( 0 == lua_gettop(L),"lua stack dirty" );

    // 根据参数从lua获取对应的玩家id
    lua_pushcfunction( L,traceback );
    lua_getglobal( L,"clt_multicast_new" );
    lua_pushinteger( L,mask );
    for ( int32_t idx = 2;idx < count + 2;idx ++ )
    {
        lua_pushinteger( L,*(raw_list + idx) );
    }
    if ( EXPECT_FALSE( LUA_OK != lua_pcall( L,count + 1,1,1 ) ) )
    {
        ERROR( "clt_multicast_new:%s",lua_tostring( L,-1 ) );

        lua_settop( L,0 ); /* remove traceback and error object */
        return;
    }
    if ( !lua_istable( L,-1 ) )
    {
        ERROR( "clt_multicast_new do NOT return a table" );
        lua_settop( L,0 );
        return;
    }
    lua_pushnil(L);  /* first key */
    while ( lua_next(L, -2) != 0 )
    {
        if ( !lua_isinteger( L,-1 ) )
        {
            lua_settop( L,0 );
            ERROR( "ssc_multicast list expect integer" );
            return;
        }
        Owner owner = static_cast<Owner>( lua_tointeger( L,-1 ) );
        ssc_one_multicast( owner,header->_cmd,header->_errno,ctx,size );

        lua_pop( L,1 );
    }
    lua_settop( L,0 ); /* remove traceback */
}
