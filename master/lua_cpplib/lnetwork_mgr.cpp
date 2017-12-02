#include "lnetwork_mgr.h"

#include "ltools.h"
#include "lstate.h"
#include "../net/codec/codec_mgr.h"
#include "../net/packet/http_packet.h"
#include "../net/packet/stream_packet.h"

const static char *ACCEPT_EVENT[] =
{
    NULL, // CNT_NONE
    "cscn_accept_new", // CNT_CSCN
    "sccn_accept_new", // CNT_SCCN
    "sscn_accept_new", // CNT_SSCN
    "http_accept_new", // CNT_HTTP
    "webs_accept_new", // CNT_WEBS
};

const static char *CONNECT_EVENT[] =
{
    NULL, // CNT_NONE
    "cscn_connect_new", // CNT_CSCN
    "sccn_connect_new", // CNT_SCCN
    "sscn_connect_new", // CNT_SSCN
    "http_connect_new", // CNT_HTTP
    "webs_connect_new", // CNT_WEBS
};

const static char *DELETE_EVENT[] =
{
    NULL, // CNT_NONE
    "cscn_connect_del", // CNT_CSCN
    "sccn_connect_del", // CNT_SCCN
    "sscn_connect_del", // CNT_SSCN
    "http_connect_del", // CNT_HTTP
    "webs_connect_del", // CNT_WEBS
};

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
uint32 lnetwork_mgr::new_connect_id()
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

    uint32 conn_id = new_connect_id();
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

    uint32 conn_id = new_connect_id();
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
owner_t lnetwork_mgr::get_owner_by_conn_id( uint32 conn_id ) const
{
    map_t<uint32,owner_t>::const_iterator itr = _conn_owner_map.find( conn_id );
    if ( itr == _conn_owner_map.end() )
    {
        return 0;
    }

    return itr->second;
}

/* 通过所有者查找连接id */
uint32 lnetwork_mgr::get_conn_id_by_owner( owner_t owner ) const
{
    map_t<owner_t,uint32>::const_iterator itr = _owner_map.find( owner );
    if ( itr == _owner_map.end() )
    {
        return 0;
    }

    return itr->second;
}

/* 通过session获取socket连接 */
class socket *lnetwork_mgr::get_conn_by_session( int32 session ) const
{
    map_t<int32,uint32>::const_iterator itr = _session_map.find( session );
    if ( itr == _session_map.end() ) return NULL;

    socket_map_t::const_iterator sk_itr = _socket_map.find( itr->second );
    if ( sk_itr == _socket_map.end() ) return NULL;

    return sk_itr->second;
}

/* 通过conn_id获取socket连接 */
class socket *lnetwork_mgr::get_conn_by_conn_id( uint32 conn_id ) const
{
    socket_map_t::const_iterator itr = _socket_map.find( conn_id );
    if ( itr == _socket_map.end() ) return NULL;

    return itr->second;
}

/* 通过conn_id获取session */
int32 lnetwork_mgr::get_session_by_conn_id( uint32 conn_id ) const
{
    map_t<uint32,int32>::const_iterator itr = _conn_session_map.find( conn_id );
    if ( itr == _conn_session_map.end() ) return 0;

    return itr->second;
}

/* 加载schema文件 */
int32 lnetwork_mgr::load_one_schema()
{
    int32 type = luaL_checkinteger( L,1 );
    const char *path = luaL_checkstring( L,2 );

    if ( type < codec::CDC_NONE || type >= codec::CDC_MAX ) return -1;

    int32 count = codec_mgr::instance()->
        load_one_schema( static_cast<codec::codec_t>(type),path );

    lua_pushinteger( L,count );
    return 1;
}

/* 设置(客户端)连接所有者 */
int32 lnetwork_mgr::set_conn_owner()
{
    uint32 conn_id = static_cast<uint32>( luaL_checkinteger( L,1) );
    owner_t owner  = luaL_checkinteger( L,2 );

    const class socket *sk = get_conn_by_conn_id( conn_id );
    if ( !sk )
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
int32 lnetwork_mgr::set_conn_session()
{
    uint32 conn_id = static_cast<uint32>( luaL_checkinteger( L,1) );
    int32 session  = luaL_checkinteger( L,2 );

    const class socket *sk = get_conn_by_conn_id( conn_id );
    if ( !sk )
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

/* 设置当前进程的session */
int32 lnetwork_mgr::set_curr_session()
{
    _session = luaL_checkinteger( L,1 );
    return 0;
}

class packet *lnetwork_mgr::lua_check_packet( socket::conn_t conn_ty )
{
    uint32 conn_id = static_cast<uint32>( luaL_checkinteger( L,1 ) );

    class socket *sk = get_conn_by_conn_id( conn_id );
    if ( !sk )
    {
        luaL_error( L,"invalid socket" );
        return NULL;
    }

    // CNT_NONE表示不需要检测连接类型
    if ( socket::CNT_NONE != conn_ty && conn_ty != sk->conn_type() )
    {
        luaL_error( L,"illegal socket connecte type" );
        return NULL;
    }

    class packet *pkt = sk->get_packet();
    if ( !pkt )
    {
        luaL_error( L,"no packet found" );
        return NULL;
    }

    return pkt;
}

/* 发送c2s数据包
 * network_mgr:send_srv_packet( conn_id,cmd,pkt )
 */
int32 lnetwork_mgr::send_srv_packet()
{
    class packet *pkt = lua_check_packet( socket::CNT_NONE );
    pkt->pack_srv( L,2 );

    return 0;
}


/* 发送s2c数据包
 * network_mgr:send_clt_packet( conn_id,cmd,errno,pkt )
 */
int32 lnetwork_mgr::send_clt_packet()
{
    class packet *pkt = lua_check_packet( socket::CNT_NONE );
    pkt->pack_clt( L,2 );

    return 0;
}

/* 发送s2s数据包
 * network_mgr:send_s2s_packet( conn_id,cmd,errno,pkt )
 */
int32 lnetwork_mgr::send_s2s_packet()
{
    class packet *pkt = lua_check_packet( socket::CNT_SSCN );

    // s2s数据包只有stream_packet能打包
    if ( packet::PKT_STREAM != pkt->type() )
    {
        return luaL_error( L,"illegal packet type" );
    }

    // maybe to slower
    // class stream_packet *stream_pkt = dynamic_cast<class stream_packet *>(pkt);
    // if ( !stream_pkt )
    // {
    //     return luaL_error( L,"not a stream packet" );
    // }
    (reinterpret_cast<stream_packet *>(pkt))->pack_ss( L,2 );

    return 0;
}

/* 在非网关发送客户端数据包
 * conn_id必须为网关连接
 * network_mgr:send_ssc_packet( conn_id,pid,cmd,errno,pkt )
 */
int32 lnetwork_mgr::send_ssc_packet()
{
    class packet *pkt = lua_check_packet( socket::CNT_SSCN );

    // s2s数据包只有stream_packet能打包
    if ( packet::PKT_STREAM != pkt->type() )
    {
        return luaL_error( L,"illegal packet type" );
    }

    (reinterpret_cast<stream_packet *>(pkt))->pack_ssc( L,2 );

    return 0;
}

/* 获取http报文头数据 */
int32 lnetwork_mgr::get_http_header()
{
    uint32 conn_id = static_cast<uint32>( luaL_checkinteger( L,1 ) );

    class socket *sk = get_conn_by_conn_id( conn_id );
    if ( !sk )
    {
        return luaL_error( L,"invalid socket" );
    }

    const class packet *pkt = sk->get_packet();
    if ( !pkt || packet::PKT_HTTP != pkt->type() )
    {
        return luaL_error( L,"illegal socket packet type" );
    }

    int32 size = 
        (reinterpret_cast<const class http_packet *>(pkt))->unpack_header( L );
    if ( size < 0 )
    {
        return luaL_error( L,"http unpack header error" );
    }

    return size;
}

/* 发送rpc数据包
 * network_mgr:send_rpc_packet( conn_id,unique_id,name,param1,param2,param3 )
 */
int32 lnetwork_mgr::send_rpc_packet()
{
    class packet *pkt = lua_check_packet( socket::CNT_SSCN );

    // s2s数据包只有stream_packet能打包
    if ( packet::PKT_STREAM != pkt->type() )
    {
        return luaL_error( L,"illegal packet type" );
    }

    (reinterpret_cast<stream_packet *>(pkt))->pack_rpc( L,2 );

    return 0;
}

/* 发送原始数据包
 * network_mgr:send_raw_packet( conn_id,content )
 */
int32 lnetwork_mgr::send_raw_packet()
{
    uint32 conn_id = static_cast<uint32>( luaL_checkinteger( L,1 ) );
    class socket *sk = get_conn_by_conn_id( conn_id );
    if ( !sk || sk->fd() <= 0 )
    {
        luaL_error( L,"invalid socket" );
        return 0;
    }

    size_t size = 0;
    const char *ctx = luaL_checklstring( L,2,&size );
    if ( !ctx ) return 0;

    class buffer &send = sk->send_buffer();
    if ( !send.reserved( size ) )
    {
        return luaL_error( L,"can not reserved buffer" );
    }

    send.__append( ctx ,size );
    sk->pending_send();

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
class socket *lnetwork_mgr::get_conn_by_owner( owner_t owner ) const
{
    uint32 dest_conn = get_conn_id_by_owner( owner );
    if ( !dest_conn ) // 客户端刚好断开或者当前进程不是网关 ?
    {
        return NULL;
    }
    socket_map_t::const_iterator itr = _socket_map.find( dest_conn );
    if ( itr == _socket_map.end() )
    {
        return NULL;
    }

    return itr->second;
}

/* 新增连接 */
class socket *lnetwork_mgr::accept_new( socket::conn_t conn_ty )
{
    uint32 conn_id = new_connect_id();
    /* 新增的连接和监听的连接类型必须一样 */
    class socket *new_sk = new class socket( conn_id,conn_ty );
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

int32 lnetwork_mgr::set_conn_io() /* 设置socket的io方式 */
{
    uint32 conn_id = luaL_checkinteger( L,1 );
    int32 io_type  = luaL_checkinteger( L,2 );

    class socket *sk = get_conn_by_conn_id( conn_id );
    if ( !sk )
    {
        return luaL_error( L,"invalid conn id" );
    }

    if ( io_type < io::IOT_NONE || io_type >= io::IOT_MAX )
    {
        return luaL_error( L,"invalid io type" );
    }

    sk->set_io( static_cast<io::io_t>(io_type) );

    return 0;
}

int32 lnetwork_mgr::set_conn_codec() /* 设置socket的编译方式 */
{
    uint32 conn_id = luaL_checkinteger( L,1 );
    int32 codec_type  = luaL_checkinteger( L,2 );

    class socket *sk = get_conn_by_conn_id( conn_id );
    if ( !sk )
    {
        return luaL_error( L,"invalid conn id" );
    }

    if ( codec_type < codec::CDC_NONE || codec_type >= codec::CDC_MAX )
    {
        return luaL_error( L,"invalid codec type" );
    }

    sk->set_codec_type( static_cast<codec::codec_t>( codec_type ) );

    return 0;
}

int32 lnetwork_mgr::set_conn_packet() /* 设置socket的打包方式 */
{
    uint32 conn_id = luaL_checkinteger( L,1 );
    int32 packet_type  = luaL_checkinteger( L,2 );

    class socket *sk = get_conn_by_conn_id( conn_id );
    if ( !sk )
    {
        return luaL_error( L,"invalid conn id" );
    }

    if ( packet_type < packet::PKT_NONE || packet_type >= packet::PKT_MAX )
    {
        return luaL_error( L,"invalid packet type" );
    }

    sk->set_packet( static_cast<packet::packet_t>( packet_type ) );
    return 0;
}