#include "lnetwork_mgr.h"

#include "ltools.h"
#include "lstate.h"
#include "../net/codec.h"
#include "../net/socket.h"

class lnetwork_mgr *lnetwork_mgr::_network_mgr = NULL;

void lnetwork_mgr::uninstance()
{
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

/* 删除无效的连接 */
void lnetwork_mgr::invoke_delete()
{
    std::vector<uint32>::iterator itr = _deleting.begin();
    for ( ;itr != _deleting.end();itr ++ )
    {
        socket_map_t::iterator sk_itr = _socket_map.find( *itr );
        if ( sk_itr == _socket_map.end() )
        {
            ERROR( "no socket to delete" );
            continue;
        }

        const class socket *sk = sk_itr->second;
        assert( "delete socket illegal",NULL != sk && sk->fd() <= 0 );

        delete sk;
        _socket_map.erase( sk_itr );
    }

    _deleting.clear();
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

    snprintf( cfg._schema,MAX_SCHEMA_NAME,"%s",schema );
    snprintf( cfg._object,MAX_SCHEMA_NAME,"%s",object );

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

    snprintf( cfg._schema,MAX_SCHEMA_NAME,"%s",schema );
    snprintf( cfg._object,MAX_SCHEMA_NAME,"%s",object );

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

    snprintf( cfg._schema,MAX_SCHEMA_NAME,"%s",schema );
    snprintf( cfg._object,MAX_SCHEMA_NAME,"%s",object );

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
    class socket *_socket = 
        new class socket( conn_id,static_cast<socket::conn_t>(conn_type) );

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
    class socket *_socket = 
        new class socket( conn_id,static_cast<socket::conn_t>(conn_type) );

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

/* 获取客户端指令配置 */
const cmd_cfg_t *lnetwork_mgr::get_cs_cmd( int32 cmd ) const
{
    cmd_map_t::const_iterator itr = _cs_cmd_map.find( cmd );
    if ( itr == _cs_cmd_map.end() ) return NULL;

    return &(itr->second);
}

/* 获取服务端指令配置 */
const cmd_cfg_t *lnetwork_mgr::get_ss_cmd( int32 cmd ) const
{
    cmd_map_t::const_iterator itr = _ss_cmd_map.find( cmd );
    if ( itr == _ss_cmd_map.end() ) return NULL;

    return &(itr->second);
}

/* 获取sc指令配置 */
const cmd_cfg_t *lnetwork_mgr::get_sc_cmd( int32 cmd ) const
{
    cmd_map_t::const_iterator itr = _sc_cmd_map.find( cmd );
    if ( itr == _sc_cmd_map.end() ) return NULL;

    return &(itr->second);
}

/* 通过连接id查找所有者 */
owner_t lnetwork_mgr::get_owner( uint32 conn_id ) const
{
    map_t<uint32,owner_t>::const_iterator itr = _conn_owner_map.find( conn_id );
    if ( itr == _conn_owner_map.end() )
    {
        return 0;
    }

    return itr->second;
}

/* 通过所有者查找连接id */
uint32 lnetwork_mgr::get_conn_id( owner_t owner ) const
{
    map_t<owner_t,uint32>::const_iterator itr = _owner_map.find( owner );
    if ( itr == _owner_map.end() )
    {
        return 0;
    }

    return itr->second;
}

/* 通过session获取socket连接 */
class socket *lnetwork_mgr::get_connection( int32 session ) const
{
    map_t<int32,uint32>::const_iterator itr = _session_map.find( session );
    if ( itr == _session_map.end() ) return NULL;

    socket_map_t::const_iterator sk_itr = _socket_map.find( itr->second );
    if ( sk_itr == _socket_map.end() ) return NULL;

    return sk_itr->second;
}

/* 通过conn_id获取session */
int32 lnetwork_mgr::get_session( uint32 conn_id ) const
{
    map_t<uint32,int32>::const_iterator itr = _conn_session_map.find( conn_id );
    if ( itr == _conn_session_map.end() ) return 0;

    return itr->second;
}

/* 加载schema文件 */
int32 lnetwork_mgr::load_schema()
{
    const char *path = luaL_checkstring( L,1 );

    int32 count = codec::load_schema( path );

    lua_pushinteger( L,count );
    return 1;
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
    // packet::instance()->unparse_c2s( 
    //     L,3,cmd,cfg._schema,cfg._object,sk->send_buffer() );

    // sk->pending_send();

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
    // packet::instance()->unparse_s2c( 
    //     L,4,cmd,ecode,cfg._schema,cfg._object,sk->send_buffer() );

    // sk->pending_send();

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
    // packet::instance()->unparse_s2s( L,pkt_index,_session,
    //     cmd,ecode,cfg._schema,cfg._object,sk->send_buffer() );

    // sk->pending_send();

    return 0;
}

/* 设置当前进程的session */
int32 lnetwork_mgr::set_curr_session()
{
    _session = luaL_checkinteger( L,1 );
    return 0;
}


/* 在非网关发送客户端数据包
 * conn_id必须为网关连接
 * network_mgr:send_ssc_packet( conn_id,pid,cmd,errno,pkt )
 */
int32 lnetwork_mgr::send_ssc_packet()
{
    uint32 conn_id = static_cast<uint32>( luaL_checkinteger( L,1 ) );
    owner_t owner  = luaL_checkinteger( L,2 );
    int32 cmd      = luaL_checkinteger( L,3 );
    int32 ecode    = luaL_checkinteger( L,4 );

    int32 pkt_index = 5;
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

    if ( socket::CNT_SSCN != sk->conn_type() )
    {
        return luaL_error( L,"illegal socket connecte type" );
    }

    const cmd_cfg_t &cfg = cmd_itr->second;
    // packet::instance()->unparse_ssc( L,pkt_index,owner,
    //     cmd,ecode,cfg._schema,cfg._object,sk->send_buffer() );

    // sk->pending_send();

    return 0;
}

/* 发送http数据包 */
int32 lnetwork_mgr::send_http_packet()
{
    uint32 conn_id = static_cast<uint32>( luaL_checkinteger( L,1 ) );

    size_t size = 0;
    const char *content = luaL_checklstring( L,2,&size );
    if ( !content || size == 0 )
    {
        return luaL_error( L,"invalid http content" );
    }

    socket_map_t::iterator itr = _socket_map.find( conn_id );
    if ( itr == _socket_map.end() )
    {
        return luaL_error( L,"no such socket found" );
    }

    class socket *sk = itr->second;
    if ( !sk or sk->fd() <= 0 )
    {
        return luaL_error( L,"invalid socket" );
    }

    if ( socket::CNT_HTTP != sk->conn_type() )
    {
        return luaL_error( L,"illegal socket connecte type" );
    }

    if ( !sk->append( content,size ) )
    {
        return luaL_error( L,"can not reserve memory" );
    }

    return 0;
}

/* 获取http报文头数据 */
int32 lnetwork_mgr::get_http_header()
{
    uint32 conn_id = static_cast<uint32>( luaL_checkinteger( L,1 ) );

    socket_map_t::iterator itr = _socket_map.find( conn_id );
    if ( itr == _socket_map.end() )
    {
        return luaL_error( L,"no such socket found" );
    }

    class socket *sk = itr->second;
    if ( !sk or sk->fd() <= 0 )
    {
        return luaL_error( L,"invalid socket" );
    }

    if ( socket::CNT_HTTP != sk->conn_type() )
    {
        return luaL_error( L,"illegal socket connecte type" );
    }

    // const class http_socket *http_sk = 
    //     static_cast<const class http_socket *>( sk );
    // const http_socket::http_info &info = http_sk->get_http();

    // lua_pushboolean( L,http_sk->upgrade() );
    // lua_pushinteger( L,http_sk->status()  );
    // lua_pushstring ( L,http_sk->method()  );

    // lua_newtable( L );
    // http_socket::head_map_t::const_iterator head_itr = info._head_field.begin();
    // for ( ;head_itr != info._head_field.end(); head_itr++ )
    // {
    //     lua_pushstring( L,head_itr->second.c_str() );
    //     lua_setfield  ( L,-2,head_itr->first.c_str() );
    // }

    return 4;
}

/* 发送rpc数据包
 * network_mgr:send_rpc_packet( conn_id,unique_id,name,param1,param2,param3 )
 */
int32 lnetwork_mgr::send_rpc_packet()
{
    uint32 conn_id  = static_cast<uint32>( luaL_checkinteger( L,1 ) );
    int32 unique_id = luaL_checkinteger( L,1 );

    socket_map_t::iterator itr = _socket_map.find( conn_id );
    if ( itr == _socket_map.end() )
    {
        return luaL_error( L,"no such socket found" );
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

    // packet::instance()->unparse_rpc( L,unique_id,1,sk->send_buffer() );

    // sk->pending_send();

    return 0;
}

/* 设置发送缓冲区大小 */
int32 lnetwork_mgr::set_send_buffer_size()
{
    uint32 conn_id = luaL_checkinteger( L,1 );
    uint32 max     = luaL_checkinteger( L,2 );
    uint32 min     = luaL_checkinteger( L,3 );

    socket_map_t::iterator itr = _socket_map.find( conn_id );
    if ( itr == _socket_map.end() )
    {
        return luaL_error( L,"no such socket found" );
    }

    class socket *_socket = itr->second;
    _socket->set_send_size( max,min );

    return 0;
}

/* 设置接收缓冲区大小 */
int32 lnetwork_mgr::set_recv_buffer_size()
{
    uint32 conn_id = luaL_checkinteger( L,1 );
    uint32 max     = luaL_checkinteger( L,2 );
    uint32 min     = luaL_checkinteger( L,3 );

    socket_map_t::iterator itr = _socket_map.find( conn_id );
    if ( itr == _socket_map.end() )
    {
        return luaL_error( L,"no such socket found" );
    }

    class socket *_socket = itr->second;
    _socket->set_recv_size( max,min );

    return 0;
}

/* 通过onwer获取socket连接 */
class socket *lnetwork_mgr::get_connection_by_owner( owner_t owner )
{
    uint32 dest_conn = network_mgr->get_conn_id( header->_owner );
    if ( !dest_conn ) // 客户端刚好断开或者当前进程不是网关 ?
    {
        return NULL;
    }
    socket_map_t::iterator itr = _socket_map.find( dest_conn );
    if ( itr == _socket_map.end() )
    {
        return NULL;
    }

    return itr->second;
}


/* 新增连接 */
class socket *lnetwork_mgr::accept_new( int32 conn_ty )
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

        lua_pop( L,2 ); /* remove traceback and error object */
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

        lua_pop( L,2 ); /* remove traceback and error object */
        return   false;
    }
    lua_pop( L,1 ); /* remove traceback */

    return true;
}
