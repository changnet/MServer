#include "dispatcher.h"

const static char *ACCEPT_EVENT[] =
{
    NULL, // CNT_NONE
    "cscn_accept_new", // CNT_CSCN
    "sccn_accept_new", // CNT_SCCN
    "sscn_accept_new", // CNT_SSCN
    "http_accept_new", // CNT_HTTP
};

const static char *CONNECT_EVENT[] =
{
    NULL, // CNT_NONE
    "cscn_connect_new", // CNT_CSCN
    "sccn_connect_new", // CNT_SCCN
    "sscn_connect_new", // CNT_SSCN
    "http_connect_new", // CNT_HTTP
};

const static char *DELETE_EVENT[] =
{
    NULL, // CNT_NONE
    "cscn_connect_del", // CNT_CSCN
    "sccn_connect_del", // CNT_SCCN
    "sscn_connect_del", // CNT_SSCN
    "http_connect_del", // CNT_HTTP
};

class dispatcher *dispatcher::_dispatcher = NULL;

void dispatcher::uninstance()
{
    delete _dispatcher;
    _dispatcher = NULL;
}
class dispatcher *dispatcher::instance()
{
    if ( !_dispatcher )
    {
        lua_State *L = lstate::instance()->state();
        _dispatcher = new class dispatcher( L );
    }

    return _dispatcher;
}

dispatcher::dispatcher( lua_State *L ) : L( L )
{
}

dispatcher::~dispatcher()
{
    L = NULL;
}

/* 新增连接 */
bool dispatcher::accept_new( int32 conn_ty,class socket *new_sk )
{
    uint32 conn_id = network_mgr->generate_connect_id();
    /* 新增的连接和监听的连接类型必须一样 */
    class socket *new_sk = new class socket( conn_id,_conn_ty );
    _socket_map[conn_id] = new_sk;

    lua_pushcfunction( L,traceback );

    lua_getglobal( L,ACCEPT_EVENT[conn_ty] );
    lua_pushinteger( L,conn_id );

    if ( expect_false( LUA_OK != lua_pcall( L,1,0,1 ) ) )
    {
        /* 出错后，无法得知脚本能否继续处理此连接
         * 为了防止死链，这里直接删除此连接
         */
        _deleting.push_back( conn_id );
        ERROR( "accept new socket:%s",lua_tostring( L,-1 ) );

        lua_pop( L,2 ); /* remove traceback and error object */
        return    NULL;
    }
    lua_pop( L,1 ); /* remove traceback */

    return new_sk;
}

/* 连接回调 */
bool dispatcher::connect_new( uint32 conn_id,int32 conn_ty,int32 ecode )
{
    lua_pushcfunction( L,traceback );

    lua_getglobal( L,CONNECT_EVENT[conn_ty] );
    lua_pushinteger( L,conn_id );
    lua_pushinteger( L,ecode   );

    if ( expect_false( LUA_OK != lua_pcall( L,2,0,1 ) ) )
    {
        /* 出错后，无法得知脚本能否继续处理此连接
         * 为了防止死链，这里直接删除此连接
         */
        _deleting.push_back( conn_id );
        ERROR( "connect_new:%s",lua_tostring( L,-1 ) );

        lua_pop( L,2 ); /* remove traceback and error object */
        return   false;
    }
    lua_pop( L,1 ); /* remove traceback */

    if ( 0 != ecode ) _deleting.push_back( conn_id );

    return true;
}


/* 断开回调 */
bool dispatcher::connect_del( uint32 conn_id,int32 conn_ty )
{
    _deleting.push_back( conn_id );

    lua_pushcfunction( L,traceback );

    lua_getglobal( L,DELETE_EVENT[conn_ty] );
    lua_pushinteger( L,conn_id );

    if ( expect_false( LUA_OK != lua_pcall( L,1,0,1 ) ) )
    {
        ERROR( "connect_del:%s",lua_tostring( L,-1 ) );

        lua_pop( L,2 ); /* remove traceback and error object */
        return   false;
    }
    lua_pop( L,1 ); /* remove traceback */

    return true;
}


/* 新数据包 */
void dispatcher::command_new( 
    uint32 conn_id,socket::conn_t conn_ty,const buffer &recv,packet *unpacker,codec *decoder )
{
    int64 target;
    packet::forward_t forwarding = unpacker->get_forward_info( target );
    // 不需要转发的，则在当前进程触发
    if ( forwarding == packet::FWT_NONE )
    {
        command_local();
        return;
    }

    // 转发给客户端
    if ( forwarding == packet::fWR_CLT )
    {
        return;
    }
    // 转发给服务器

}

void dispatcher::command_local( packet *unpacker,codec *decoder )
{
    assert( "lua stack dirty",0 == lua_gettop( L ) );

    int32 cmd = unpacker->get_cmd();

    lua_pushcfunction( L,traceback );

    // 协议头参数入栈
    int32 packet_args = unpacker->push_args( L );
    if (packet_args < 0 )
    {
        lua_pop( L,1 ); /* remove traceback */
        return;
    }

    // 协议内容解码后入栈
    int32 codec_args = decoder->decode();

    int32 args = packet_args + codec_args - 1; // 不包含函数本身
    if ( expect_false( LUA_OK != lua_pcall( L,,0,1 ) ) )
    {
        ERROR( "invoke_command_local:%s",lua_tostring( L,-1 ) );

        lua_pop( L,2 ); /* remove traceback and error object */
        return;
    }
    lua_pop( L,1 ); /* remove traceback */
}

/* 处理客户端发给服务器数据包 */
void dispatcher::process_command( uint32 conn_id,const c2s_header *header )
{
    const cmd_cfg_t *cmd_cfg = get_cs_cmd( header->_cmd );
    if ( !cmd_cfg )
    {
        ERROR( "c2s cmd(%d) no cmd cfg found",header->_cmd );
        return;
    }

    /* 这个指令不是在当前进程处理，自动转发到对应进程 */
    if ( cmd_cfg->_session != _session )
    {
        clt_forwarding( conn_id,header,cmd_cfg->_session );
        return;
    }

    /* 在当前进程处理 */
    owner_t owner = get_owner( conn_id );
    cs_command( conn_id,owner,cmd_cfg,header );
}

/* 处理服务器发给客户端数据包 */
void dispatcher::process_command( uint32 conn_id,const s2c_header *header )
{
    const cmd_cfg_t *cmd_cfg = get_sc_cmd( header->_cmd );
    if ( !cmd_cfg )
    {
        ERROR( "s2c cmd(%d) no cmd cfg found",header->_cmd );
        return;
    }

    sc_command( conn_id,cmd_cfg,header );
}

/* 解析网关转发的客户端数据包 */
void dispatcher::process_css_cmd( uint32 conn_id,const s2s_header *header )
{
    const struct c2s_header *clt_header = 
        reinterpret_cast<const struct c2s_header *>( header + 1 );
    const cmd_cfg_t *clt_cfg = get_cs_cmd( clt_header->_cmd );
    if ( !clt_cfg )
    {
        ERROR( "c2s cmd(%d) no cmd cfg found",clt_header->_cmd );
        return;
    }
    cs_command( conn_id,header->_owner,clt_cfg,clt_header,true );
}

/* 解析其他服务器转发到网关的数据包 */
void dispatcher::process_ssc_cmd( uint32 conn_id,const s2s_header *header )
{
    uint32 dest_conn = get_conn_id( header->_owner );
    if ( !dest_conn ) // 客户端刚好断开或者当前进程不是网关 ?
    {
        ERROR( "ssc packet no clt connect found" );
        return;
    }
    socket_map_t::iterator itr = _socket_map.find( dest_conn );
    if ( itr == _socket_map.end() )
    {
        ERROR( "ssc packet no clt connect not exist" );
        return;
    }
    class socket *sk = itr->second;
    if ( socket::CNT_SCCN != sk->conn_type() )
    {
        ERROR( "ssc packet destination conn is not a clt" );
        return;
    }

    class buffer &send = sk->send_buffer();
    bool is_ok = send.append( header + 1,PACKET_BUFFER_LEN(header) );
    if ( !is_ok )
    {
        ERROR( "ssc packet can not append buffer" );
        return;
    }
    sk->pending_send();
}

/* 处理服务器之间数据包 */
void dispatcher::process_command( uint32 conn_id,const s2s_header *header )
{
    /* 先判断数据包类型 */
    switch ( header->_mask )
    {
        case packet::PKT_SSPK : break;// 服务器数据包放在后面处理
        // 网关转发的客户端包
        case packet::PKT_CSPK : process_css_cmd( conn_id,header );return;
        // 需要转发给客户端的数据包
        case packet::PKT_SCPK : process_ssc_cmd( conn_id,header );return;
        case packet::PKT_RPCS : process_rpc_cmd( conn_id,header );return;
        case packet::PKT_RPCR : process_rpc_return( conn_id,header );return;
        default :
        {
            ERROR( "unknow server packet:"
                "cmd %d,mask %d",header->_cmd,header->_mask );
            return;
        }
    }

    const cmd_cfg_t *cmd_cfg = get_ss_cmd( header->_cmd );
    if ( !cmd_cfg )
    {
        ERROR( "s2s cmd(%d) no cmd cfg found",header->_cmd );
        return;
    }

    /* 这个指令不是在当前进程处理，自动转发到对应进程 */
    if ( cmd_cfg->_session != _session )
    {
        class socket *dest_sk  = get_connection( cmd_cfg->_session );
        if ( !dest_sk )
        {
            ERROR( "server packet forwarding "
                "no destination found.cmd:%d",header->_cmd );
            return;
        }

        bool is_ok = dest_sk->append( header,PACKET_LENGTH( header ) );
        if ( !is_ok )
        {
            ERROR( "server packet forwrding "
                "can not reserved memory:%ld",int64(PACKET_LENGTH( header )) );
        }
        return;
    }

    ss_command( conn_id,cmd_cfg,header );
}

/* 转客户端数据包 */
void dispatcher::clt_forwarding( 
    uint32 conn_id,const c2s_header *header,int32 session )
{
    class socket *dest_sk  = get_connection( session );
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
    s2sh._mask   = packet::PKT_CSPK;
    s2sh._owner  = get_owner( conn_id );

    send.__append( &s2sh,sizeof(struct s2s_header) );
    send.__append( header,size );

    dest_sk->pending_send();
}

/* 数据包回调脚本 */
void dispatcher::cs_command( uint32 conn_id,owner_t owner,
    const cmd_cfg_t *cfg,const c2s_header *header,bool forwarding )
{
    lua_State *L = lstate::instance()->state();
    assert( "lua stack dirty",0 == lua_gettop(L) );

    lua_pushcfunction( L,traceback );
    lua_getglobal( L,forwarding ? "css_command_new" : "cs_command_new" );
    lua_pushinteger( L,conn_id );
    lua_pushinteger( L,owner );
    lua_pushinteger( L,header->_cmd );

    int32 cnt = packet::instance()->parse( L,cfg->_schema,cfg->_object,header );
    if ( cnt < 0 )
    {
        lua_pop( L,5 );
        return;
    }

    if ( expect_false( LUA_OK != lua_pcall( L,3 + cnt,0,1 ) ) )
    {
        ERROR( "cs_command:%s",lua_tostring( L,-1 ) );

        lua_pop( L,2 ); /* remove traceback and error object */
        return;
    }
    lua_pop( L,1 ); /* remove traceback */
}

/* 数据包回调脚本 */
void dispatcher::sc_command( 
    uint32 conn_id,const cmd_cfg_t *cfg,const s2c_header *header )
{
    lua_State *L = lstate::instance()->state();
    assert( "lua stack dirty",0 == lua_gettop(L) );

    lua_pushcfunction( L,traceback );
    lua_getglobal( L,"sc_command_new" );
    lua_pushinteger( L,conn_id );
    lua_pushinteger( L,header->_cmd );
    lua_pushinteger( L,header->_errno );

    int32 cnt = packet::instance()->parse( L,cfg->_schema,cfg->_object,header );
    if ( cnt < 0 )
    {
        lua_pop( L,5 );
        return;
    }

    if ( expect_false( LUA_OK != lua_pcall( L,3 + cnt,0,1 ) ) )
    {
        ERROR( "sc_command:%s",lua_tostring( L,-1 ) );

        lua_pop( L,2 ); /* remove traceback and error object */
        return;
    }
    lua_pop( L,1 ); /* remove traceback */
}

/* 数据包回调脚本 */
void dispatcher::ss_command( 
    uint32 conn_id,const cmd_cfg_t *cfg,const s2s_header *header )
{
    lua_State *L = lstate::instance()->state();
    assert( "lua stack dirty",0 == lua_gettop( L ) );

    lua_pushcfunction( L,traceback );
    lua_getglobal( L,"ss_command_new" );
    lua_pushinteger( L,conn_id );
    lua_pushinteger( L,header->_owner );
    lua_pushinteger( L,header->_cmd );
    lua_pushinteger( L,header->_errno );

    int32 cnt = packet::instance()->parse( L,cfg->_schema,cfg->_object,header );
    if ( cnt < 0 )
    {
        lua_pop( L,6 );
        return;
    }

    if ( expect_false( LUA_OK != lua_pcall( L,4 + cnt,0,1 ) ) )
    {
        ERROR( "invoke_ss_command:%s",lua_tostring( L,-1 ) );

        lua_pop( L,2 ); /* remove traceback and error object */
        return;
    }
    lua_pop( L,1 ); /* remove traceback */
}


/* 新http请求 */
void dispatcher::http_command_new( const class socket *sk )
{
    assert( "not a http socket",socket::CNT_HTTP == sk->conn_type() );

    const http_socket *http_sk = static_cast<const http_socket *>( sk );

    lua_State *L = lstate::instance()->state();
    assert( "lua stack dirty",0 == lua_gettop(L) );

    const struct http_socket::http_info &info = http_sk->get_http();

    lua_pushcfunction( L,traceback );
    lua_getglobal    ( L,"http_command_new" );
    lua_pushinteger  ( L,http_sk->conn_id() );
    lua_pushstring   ( L,info._url.c_str()  );
    lua_pushstring   ( L,info._body.c_str() );

    if ( expect_false( LUA_OK != lua_pcall( L,3,0,1 ) ) )
    {
        ERROR( "http_command_new:%s",lua_tostring( L,-1 ) );

        lua_pop( L,2 ); /* remove traceback and error object */
        return;
    }
    lua_pop( L,1 ); /* remove traceback */
}


/* 处理rpc调用 */
void dispatcher::process_rpc_cmd( uint32 conn_id,const s2s_header *header )
{
    lua_State *L = lstate::instance()->state();
    assert( "lua stack dirty",0 == lua_gettop(L) );

    lua_pushcfunction( L,traceback );
    lua_getglobal( L,"rpc_command_new" );
    lua_pushinteger( L,conn_id );
    lua_pushinteger( L,header->_owner );

    int32 cnt = packet::instance()->parse( L,header );
    if ( cnt < 1 )
    {
        lua_pop( L,4 + cnt );
        ERROR( "rpc command miss function name" );
        return;
    }

    int32 top = lua_gettop( L );
    int32 unique_id = static_cast<int32>( header->_owner );
    int32 ecode = lua_pcall( L,4 + cnt,LUA_MULTRET,1 );
    if ( unique_id > 0 )
    {
        socket_map_t::iterator itr = _socket_map.find( conn_id );
        if ( itr != _socket_map.end() )
        {
            class socket *sk = itr->second;
            packet::instance()->unparse_rpc( 
                L,unique_id,ecode,top,sk->send_buffer() );
            sk->pending_send();
        }
        else
        {
            ERROR( "process_rpc_cmd no return socket found" );
        }
    }

    if ( LUA_OK != ecode )
    {
        ERROR( "rpc cmd error:%s",lua_tostring(L,-1) );
        lua_pop( L,2 ); /* remove error object and traceback */
        return;
    }

    lua_pop( L,1 ); /* remove trace back */
}

/* 处理rpc返回 */
void dispatcher::process_rpc_return( uint32 conn_id,const s2s_header *header )
{
    lua_State *L = lstate::instance()->state();
    assert( "lua stack dirty",0 == lua_gettop(L) );

    lua_pushcfunction( L,traceback );
    lua_getglobal( L,"rpc_command_return" );
    lua_pushinteger( L,conn_id );
    lua_pushinteger( L,header->_owner );
    lua_pushinteger( L,header->_errno );

    int32 cnt = packet::instance()->parse( L,header );
    if ( LUA_OK != lua_pcall( L,3 + cnt,0,1 ) )
    {
        ERROR( "process_rpc_return:%s",lua_tostring( L,-1 ) );

        lua_pop( L,2 ); /* remove traceback and error object */
        return;
    }
    lua_pop( L,1 ); /* remove traceback */

    return;
}
