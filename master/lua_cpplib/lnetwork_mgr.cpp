#include "lnetwork_mgr.h"

#include "ltools.h"
#include "lstate.h"
#include "../net/packet.h"
#include "../net/stream_socket.h"

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

const static char *COMMAND_EVENT[] =
{
    NULL, // CNT_NONE
    "cscn_command_new", // CNT_CSCN
    "sccn_command_new", // CNT_SCCN
    "sscn_command_new", // CNT_SSCN
    "http_command_new", // CNT_HTTP
};

const static char *DELETE_EVENT[] =
{
    NULL, // CNT_NONE
    "cscn_connect_del", // CNT_CSCN
    "sccn_connect_del", // CNT_SCCN
    "sscn_connect_del", // CNT_SSCN
    "http_connect_del", // CNT_HTTP
};

class lnetwork_mgr *lnetwork_mgr::_network_mgr = NULL;

void lnetwork_mgr::uninstance()
{
    packet::uninstance();

    delete _network_mgr;
    _network_mgr = NULL;
}

class lnetwork_mgr *lnetwork_mgr::instance()
{
    if ( NULL == _network_mgr )
    {
        lua_State *L = lstate::instance()->state();
        assert( "NULL lua state",L );
        _network_mgr = new lnetwork_mgr( L );
    }

    return _network_mgr;
}

lnetwork_mgr::~lnetwork_mgr()
{
    socket_map_t::iterator itr = _socket_map.begin();
    for ( ;itr != _socket_map.end(); itr ++ )
    {
        class socket *sk = itr->second;
        if ( sk ) sk->stop();
        delete sk;
    }

    _owner_map.clear();
    _socket_map.clear();
    _session_map.clear();
    _conn_owner_map.clear();
    _conn_session_map.clear();
}

lnetwork_mgr::lnetwork_mgr( lua_State *L )
    :L(L),_conn_seed(0)
{
    assert( "lnetwork_mgr is singleton",NULL == _network_mgr );
}

/* 产生一个唯一的连接id 
 * 之所以不用系统的文件描述符fd，是因为fd对于上层逻辑不可控。比如一个fd被释放，可能在多个进程
 * 之间还未处理完，此fd就被重用了。当前的连接id不太可能会在短时间内重用。
 */
uint32 lnetwork_mgr::generate_connect_id()
{
    do
    {
        if ( 0xFFFFFFFF <= _conn_seed ) _conn_seed = 0;
        _conn_seed ++;
    } while ( _socket_map.end() != _socket_map.find( _conn_seed ) );

    return _conn_seed;
}

/* 设置某个客户端指令的参数
 * network_mgr:set_cs_cmd( cmd,schema,object[,mask,session] )
 */
int32 lnetwork_mgr::set_cs_cmd()
{
    int32 cmd          = luaL_checkinteger( L,1 );
    const char *schema = luaL_checkstring ( L,2 );
    const char *object = luaL_checkstring ( L,3 );
    int32 mask         = luaL_optinteger  ( L,4,0 );
    int32 session      = luaL_optinteger  ( L,5,_session );

    cmd_cfg_t &cfg = _cs_cmd_map[cmd];
    cfg._cmd     = cmd;
    cfg._mask    = mask;
    cfg._session = session;

    snprintf( cfg._schema,MAX_SCHEMA_NAME,schema );
    snprintf( cfg._object,MAX_SCHEMA_NAME,object );

    return 0;
}

/* 设置某个服务器指令的参数
 * network_mgr:set_ss_cmd( cmd,schema,object[,mask,session] )
 */
int32 lnetwork_mgr::set_ss_cmd()
{
    int32 cmd          = luaL_checkinteger( L,1 );
    const char *schema = luaL_checkstring ( L,2 );
    const char *object = luaL_checkstring ( L,3 );
    int32 mask         = luaL_optinteger  ( L,4,0 );
    int32 session      = luaL_optinteger  ( L,5,_session );

    cmd_cfg_t &cfg = _ss_cmd_map[cmd];
    cfg._cmd     = cmd;
    cfg._mask    = mask;
    cfg._session = session;

    snprintf( cfg._schema,MAX_SCHEMA_NAME,schema );
    snprintf( cfg._object,MAX_SCHEMA_NAME,object );

    return 0;
}

/* 设置某个sc指令的参数
 * network_mgr:set_ss_cmd( cmd,schema,object[,mask,session] )
 */
int32 lnetwork_mgr::set_sc_cmd()
{
    int32 cmd          = luaL_checkinteger( L,1 );
    const char *schema = luaL_checkstring ( L,2 );
    const char *object = luaL_checkstring ( L,3 );
    int32 mask         = luaL_optinteger  ( L,4,0 );
    int32 session      = luaL_optinteger  ( L,5,_session );

    cmd_cfg_t &cfg = _sc_cmd_map[cmd];
    cfg._cmd     = cmd;
    cfg._mask    = mask;
    cfg._session = session;

    snprintf( cfg._schema,MAX_SCHEMA_NAME,schema );
    snprintf( cfg._object,MAX_SCHEMA_NAME,object );

    return 0;
}

/* 仅关闭socket，但不销毁内存
 * network_mgr:close( conn_id )
 */
int32 lnetwork_mgr::close()
{
    uint32 conn_id = luaL_checkinteger( L,1 );

    socket_map_t::iterator itr = _socket_map.find( conn_id );
    if ( itr == _socket_map.end() )
    {
        return luaL_error( L,"no such socket found" );
    }

    class socket *_socket = itr->second;
    if ( _socket->fd() < 0 )
    {
        return luaL_error( L,"try to close a invalid socket" );
    }

    /* stop尝试把缓冲区的数据直接发送 */
    _socket->stop();

    /* 这里不能删除内存，因为脚本并不知道是否会再次访问此socket
     * 比如底层正在一个for循环里回调脚本时，就需要再次访问
     */
    _deleting.push_back( conn_id );

    return 0;
}

/* 监听端口
 * network_mgr:listen( host,port,conn_type )
 */
int32 lnetwork_mgr::listen()
{
    const char *host = luaL_checkstring( L,1 );
    if ( !host )
    {
        return luaL_error( L,"host not specify" );
    }

    int32 port      = luaL_checkinteger( L,2 );
    int32 conn_type = luaL_checkinteger( L,3 );
    if ( conn_type <= socket::CNT_NONE || conn_type >= socket::CNT_MAXT )
    {
        return luaL_error( L,"illegal connection type" );
    }

    uint32 conn_id = generate_connect_id();
    class socket *_socket = new class stream_socket( 
        conn_id,static_cast<socket::conn_t>(conn_type) );

    int32 fd = _socket->listen( host,port );
    if ( fd < 0 )
    {
        delete _socket;
        return luaL_error( L,strerror(errno) );
    }

    _socket_map[conn_id] = _socket;

    lua_pushinteger( L,conn_id );
    return 1;
}

/* 新增连接 */
bool lnetwork_mgr::accept_new( 
    uint32 conn_id,int32 conn_ty,class socket *new_sk )
{
    lua_pushcfunction( L,traceback );

    lua_getglobal( L,ACCEPT_EVENT[conn_ty] );
    lua_pushinteger( L,conn_id );

    if ( expect_false( LUA_OK != lua_pcall( L,1,0,1 ) ) )
    {
        /* 出错后，无法得知脚本能否继续处理此连接
         * 为了防止死链，这里直接删除此连接
         */
        delete new_sk;
        ERROR( "accept new socket:%s",lua_tostring( L,-1 ) );

        lua_pop( L,1 ); /* remove traceback and error object */
        return   false;
    }
    lua_pop( L,1 ); /* remove traceback */

    _socket_map[conn_id] = new_sk;

    return true;
}

/* 主动连接其他服务器
 * network_mgr:connect( host,port,conn_type )
 */
int32 lnetwork_mgr::connect()
{
    const char *host = luaL_checkstring( L,1 );
    if ( !host )
    {
        return luaL_error( L,"host not specify" );
    }

    int32 port      = luaL_checkinteger( L,2 );
    int32 conn_type = luaL_checkinteger( L,3 );
    if ( conn_type <= socket::CNT_NONE || conn_type >= socket::CNT_MAXT )
    {
        return luaL_error( L,"illegal connection type" );
    }

    uint32 conn_id = generate_connect_id();
    class socket *_socket = new class stream_socket( 
        conn_id,static_cast<socket::conn_t>(conn_type) );

    int32 fd = _socket->connect( host,port );
    if ( fd < 0 )
    {
        delete _socket;
        luaL_error( L,strerror(errno) );
        return 0;
    }

    _socket_map[conn_id] = _socket;
    lua_pushinteger( L,conn_id );
    return 1;
}

/* 连接回调 */
bool lnetwork_mgr::connect_new( uint32 conn_id,int32 conn_ty,int32 ecode )
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

        lua_pop( L,1 ); /* remove traceback and error object */
        return   false;
    }
    lua_pop( L,1 ); /* remove traceback */

    if ( 0 != ecode ) _deleting.push_back( conn_id );

    return true;
}


/* 断开回调 */
bool lnetwork_mgr::connect_del( uint32 conn_id,int32 conn_ty )
{
    _deleting.push_back( conn_id );

    lua_pushcfunction( L,traceback );

    lua_getglobal( L,DELETE_EVENT[conn_ty] );
    lua_pushinteger( L,conn_id );

    if ( expect_false( LUA_OK != lua_pcall( L,1,0,1 ) ) )
    {
        ERROR( "connect_del:%s",lua_tostring( L,-1 ) );

        lua_pop( L,1 ); /* remove traceback and error object */
        return   false;
    }
    lua_pop( L,1 ); /* remove traceback */

    return true;
}

/* 获取客户端指令配置 */
const cmd_cfg_t *lnetwork_mgr::get_cs_cmd( int32 cmd )
{
    cmd_map_t::iterator itr = _cs_cmd_map.find( cmd );
    if ( itr == _cs_cmd_map.end() ) return NULL;

    return &(itr->second);
}

/* 获取服务端指令配置 */
const cmd_cfg_t *lnetwork_mgr::get_ss_cmd( int32 cmd )
{
    cmd_map_t::iterator itr = _ss_cmd_map.find( cmd );
    if ( itr == _ss_cmd_map.end() ) return NULL;

    return &(itr->second);
}

/* 获取sc指令配置 */
const cmd_cfg_t *lnetwork_mgr::get_sc_cmd( int32 cmd )
{
    cmd_map_t::iterator itr = _sc_cmd_map.find( cmd );
    if ( itr == _sc_cmd_map.end() ) return NULL;

    return &(itr->second);
}

/* 通过连接id查找所有者 */
owner_t lnetwork_mgr::get_owner( uint32 conn_id )
{
    map_t<uint32,owner_t>::iterator itr = _conn_owner_map.find( conn_id );
    if ( itr == _conn_owner_map.end() )
    {
        return 0;
    }

    return itr->second;
}

/* 通过所有者查找连接id */
uint32 lnetwork_mgr::get_conn_id( owner_t owner )
{
    map_t<owner_t,uint32>::iterator itr = _owner_map.find( owner );
    if ( itr == _owner_map.end() )
    {
        return 0;
    }

    return itr->second;
}

/* 通过session获取socket连接 */
class socket *lnetwork_mgr::get_connection( int32 session )
{
    map_t<int32,uint32>::iterator itr = _session_map.find( session );
    if ( itr == _session_map.end() ) return NULL;

    socket_map_t::iterator sk_itr = _socket_map.find( itr->second );
    if ( sk_itr == _socket_map.end() ) return NULL;

    return sk_itr->second;
}

/* 通过conn_id获取session */
int32 lnetwork_mgr::get_session( uint32 conn_id )
{
    map_t<uint32,int32>::iterator itr = _conn_session_map.find( conn_id );
    if ( itr == _conn_session_map.end() ) return 0;

    return itr->second;
}

/* 加载schema文件 */
int32 lnetwork_mgr::load_schema()
{
    const char *path = luaL_checkstring( L,1 );

    int32 count = packet::instance()->load_schema( path );

    lua_pushinteger( L,count );
    return 1;
}

/* 新数据包 */
void lnetwork_mgr::command_new( 
    uint32 conn_id,socket::conn_t conn_ty,const buffer &recv )
{
    /* 不同的链接，数据包不一样 */
    switch( conn_ty )
    {
        case socket::CNT_CSCN : /* 解析服务器发往客户端的包 */
        {
            process_command( conn_id,
                reinterpret_cast<struct s2c_header *>( recv.data() ) );
        }break;
        case socket::CNT_SCCN : /* 解析客户端发往服务器的包 */
        {
            process_command( conn_id,
                reinterpret_cast<struct c2s_header *>( recv.data() ) );
        }break;
        case socket::CNT_SSCN : /* 解析服务器发往服务器的包 */
        {
            process_command( conn_id,
                reinterpret_cast<struct s2s_header *>( recv.data() ) );
        }break;
        default : assert( "unknow socket connect type",false );return;
    }
}

/* 处理客户端发给服务器数据包 */
void lnetwork_mgr::process_command( uint32 conn_id,const c2s_header *header )
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
void lnetwork_mgr::process_command( uint32 conn_id,const s2c_header *header )
{
    const cmd_cfg_t *cmd_cfg = get_sc_cmd( header->_cmd );
    if ( !cmd_cfg )
    {
        ERROR( "s2c cmd(%d) no cmd cfg found",header->_cmd );
        return;
    }

    sc_command( conn_id,cmd_cfg,header );
}

/* 处理服务器之间数据包 */
void lnetwork_mgr::process_command( uint32 conn_id,const s2s_header *header )
{
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

    /* 在当前进程处理 */
    switch ( header->_mask )
    {
        case packet::PKT_CSPK : // 网关转发的客户端包
        {
            const struct c2s_header *clt_header = 
                reinterpret_cast<const struct c2s_header *>( header + 1 );
            const cmd_cfg_t *clt_cfg = get_cs_cmd( clt_header->_cmd );
            if ( !cmd_cfg )
            {
                ERROR( "c2s cmd(%d) no cmd cfg found",clt_header->_cmd );
                return;
            }
            cs_command( conn_id,header->_owner,clt_cfg,clt_header );
        }break;
        case packet::PKT_SSPK : // 服务器数据包
        {
            ss_command( conn_id,cmd_cfg,header );
        }break;
        default :
        {
            ERROR( "unknow server packet:"
                "cmd %d,mask %d",header->_cmd,header->_mask );
            return;
        }
    }
}

/* 转客户端数据包 */
void lnetwork_mgr::clt_forwarding( 
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
void lnetwork_mgr::cs_command( uint32 conn_id,
    owner_t owner,const cmd_cfg_t *cfg,const c2s_header *header )
{
    lua_State *L = lstate::instance()->state();
    assert( "lua stack dirty",0 == lua_gettop(L) );

    lua_pushcfunction( L,traceback );
    lua_getglobal( L,COMMAND_EVENT[socket::CNT_SCCN] );
    lua_pushinteger( L,conn_id );
    lua_pushinteger( L,owner );
    lua_pushinteger( L,header->_cmd );

    int32 cnt = packet::instance()->parse( L,cfg->_schema,cfg->_object,header );
    if ( cnt < 0 )
    {
        lua_pop( L,4 );
        return;
    }

    if ( expect_false( LUA_OK != lua_pcall( L,3 + cnt,0,1 ) ) )
    {
        ERROR( "cs_command:%s",lua_tostring( L,-1 ) );

        lua_pop( L,1 ); /* remove traceback and error object */
        return;
    }
    lua_pop( L,1 ); /* remove traceback */
}

/* 数据包回调脚本 */
void lnetwork_mgr::sc_command( 
    uint32 conn_id,const cmd_cfg_t *cfg,const s2c_header *header )
{
    lua_State *L = lstate::instance()->state();
    assert( "lua stack dirty",0 == lua_gettop(L) );

    lua_pushcfunction( L,traceback );
    lua_getglobal( L,COMMAND_EVENT[socket::CNT_CSCN] );
    lua_pushinteger( L,conn_id );
    lua_pushinteger( L,header->_cmd );
    lua_pushinteger( L,header->_errno );

    int32 cnt = packet::instance()->parse( L,cfg->_schema,cfg->_object,header );
    if ( cnt < 0 )
    {
        lua_pop( L,4 );
        return;
    }

    if ( expect_false( LUA_OK != lua_pcall( L,3 + cnt,0,1 ) ) )
    {
        ERROR( "sc_command:%s",lua_tostring( L,-1 ) );

        lua_pop( L,1 ); /* remove traceback and error object */
        return;
    }
    lua_pop( L,1 ); /* remove traceback */
}

/* 数据包回调脚本 */
void lnetwork_mgr::ss_command( 
    uint32 conn_id,const cmd_cfg_t *cfg,const s2s_header *header )
{
    lua_State *L = lstate::instance()->state();
    assert( "lua stack dirty",0 == lua_gettop(L) );

    lua_pushcfunction( L,traceback );
    lua_getglobal( L,COMMAND_EVENT[socket::CNT_SSCN] );
    lua_pushinteger( L,conn_id );
    lua_pushinteger( L,header->_owner );
    lua_pushinteger( L,header->_cmd );
    lua_pushinteger( L,header->_errno );

    int32 cnt = packet::instance()->parse( L,cfg->_schema,cfg->_object,header );
    if ( cnt < 0 )
    {
        lua_pop( L,4 );
        return;
    }

    if ( expect_false( LUA_OK != lua_pcall( L,4 + cnt,0,1 ) ) )
    {
        ERROR( "invoke_command:%s",lua_tostring( L,-1 ) );

        lua_pop( L,1 ); /* remove traceback and error object */
        return;
    }
    lua_pop( L,1 ); /* remove traceback */
}

/* 发送c2s数据包
 * network_mgr:send_c2s_packet( conn_id,cmd,pkt )
 */
int32 lnetwork_mgr::send_c2s_packet()
{
    uint32 conn_id = static_cast<uint32>( luaL_checkinteger( L,1 ) );
    int32  cmd     = luaL_checkinteger( L,2 );

    if ( !lua_istable( L,3 ) )
    {
        return luaL_error( L,
            "argument #3 expect table,got %s",lua_typename( L,lua_type(L,3) ) );
    }

    socket_map_t::iterator itr = _socket_map.find( conn_id );
    if ( itr == _socket_map.end() )
    {
        return luaL_error( L,"no such socket found" );
    }

    cmd_map_t::iterator cmd_itr = _cs_cmd_map.find( cmd );
    if ( cmd_itr == _cs_cmd_map.end() )
    {
        return luaL_error( L,"no command config found:%d",cmd );
    }

    class socket *sk = itr->second;
    if ( !sk or sk->fd() <= 0 )
    {
        return luaL_error( L,"invalid socket" );
    }

    if ( socket::CNT_CSCN != sk->conn_type() )
    {
        return luaL_error( L,"illegal socket connecte type" );
    }

    const cmd_cfg_t &cfg = cmd_itr->second;
    packet::instance()->unparse_c2s( 
        L,3,cmd,cfg._schema,cfg._object,sk->send_buffer() );

    sk->pending_send();

    return 0;
}


/* 发送s2c数据包
 * network_mgr:send_s2c_packet( conn_id,cmd,errno,pkt )
 */
int32 lnetwork_mgr::send_s2c_packet()
{
    uint32 conn_id = static_cast<uint32>( luaL_checkinteger( L,1 ) );
    int32 cmd      = luaL_checkinteger( L,2 );
    int32 ecode    = luaL_checkinteger( L,3 );
    if ( !lua_istable( L,4 ) )
    {
        return luaL_error( L,
            "argument #4 expect table,got %s",lua_typename( L,lua_type(L,4) ) );
    }

    socket_map_t::iterator itr = _socket_map.find( conn_id );
    if ( itr == _socket_map.end() )
    {
        return luaL_error( L,"no such socket found" );
    }

    cmd_map_t::iterator cmd_itr = _sc_cmd_map.find( cmd );
    if ( cmd_itr == _sc_cmd_map.end() )
    {
        return luaL_error( L,"no command config found:%d",cmd );
    }

    class socket *sk = itr->second;
    if ( !sk or sk->fd() <= 0 )
    {
        return luaL_error( L,"invalid socket" );
    }

    if ( socket::CNT_SCCN != sk->conn_type() )
    {
        return luaL_error( L,"illegal socket connecte type" );
    }

    const cmd_cfg_t &cfg = cmd_itr->second;
    packet::instance()->unparse_s2c( 
        L,4,cmd,ecode,cfg._schema,cfg._object,sk->send_buffer() );

    sk->pending_send();

    return 0;
}

/* 设置(客户端)连接所有者 */
int32 lnetwork_mgr::set_owner()
{
    uint32 conn_id = static_cast<uint32>( luaL_checkinteger( L,1) );
    owner_t owner  = luaL_checkinteger( L,2 );

    socket_map_t::iterator itr = _socket_map.find( conn_id );
    if ( itr == _socket_map.end() )
    {
        return luaL_error( L,"connnection not exist" );
    }

    const class socket *sk = itr->second;
    if ( sk->fd() <= 0 )
    {
        return luaL_error( L,"invalid connection" );
    }

    if ( socket::CNT_SCCN != sk->conn_type() )
    {
        return luaL_error( L,"illegal connection type" );
    }

    _owner_map[owner]  = conn_id;
    _conn_owner_map[conn_id] = owner;

    return 0;
}

/* 设置(服务器)连接session */
int32 lnetwork_mgr::set_session()
{
    uint32 conn_id = static_cast<uint32>( luaL_checkinteger( L,1) );
    int32 session  = luaL_checkinteger( L,2 );

    socket_map_t::iterator itr = _socket_map.find( conn_id );
    if ( itr == _socket_map.end() )
    {
        return luaL_error( L,"connnection not exist" );
    }

    const class socket *sk = itr->second;
    if ( sk->fd() <= 0 )
    {
        return luaL_error( L,"invalid connection" );
    }

    if ( socket::CNT_SSCN != sk->conn_type() )
    {
        return luaL_error( L,"illegal connection type" );
    }

    _session_map[session] = conn_id;
    _conn_session_map[conn_id] = session;

    return 0;
}

/* 发送s2s数据包
 * network_mgr:send_s2s_packet( conn_id,cmd,errno,pkt )
 */
int32 lnetwork_mgr::send_s2s_packet()
{
    uint32 conn_id = static_cast<uint32>( luaL_checkinteger( L,1 ) );
    int32 cmd      = luaL_checkinteger( L,2 );
    int32 ecode    = luaL_checkinteger( L,3 );

    int32 pkt_index = 4;
    if ( !lua_istable( L,pkt_index ) )
    {
        return luaL_error( L,"argument #%d expect table,got %s",
            pkt_index,lua_typename( L,lua_type(L,pkt_index) ) );
    }

    socket_map_t::iterator itr = _socket_map.find( conn_id );
    if ( itr == _socket_map.end() )
    {
        return luaL_error( L,"no such socket found" );
    }

    cmd_map_t::iterator cmd_itr = _ss_cmd_map.find( cmd );
    if ( cmd_itr == _ss_cmd_map.end() )
    {
        return luaL_error( L,"no command config found:%d",cmd );
    }

    class socket *sk = itr->second;
    if ( !sk or sk->fd() <= 0 )
    {
        return luaL_error( L,"invalid socket" );
    }

    if ( socket::CNT_SSCN != sk->conn_type() )
    {
        return luaL_error( L,"illegal socket connecte type" );
    }

    const cmd_cfg_t &cfg = cmd_itr->second;
    packet::instance()->unparse_s2s( L,pkt_index,_session,
        cmd,ecode,cfg._schema,cfg._object,sk->send_buffer() );

    sk->pending_send();

    return 0;
}

/* 设置当前进程的session */
int32 lnetwork_mgr::set_curr_session()
{
    _session = luaL_checkinteger( L,1 );
    return 0;
}