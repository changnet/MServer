#include "lnetwork_mgr.h"

#include "ltools.h"
#include "../system/static_global.h"

#include "../net/packet/http_packet.h"
#include "../net/packet/stream_packet.h"
#include "../net/packet/websocket_packet.h"

lnetwork_mgr::~lnetwork_mgr()
{
    clear();
}

lnetwork_mgr::lnetwork_mgr() :_conn_seed(0)
{
}

void lnetwork_mgr::clear() /* 清除所有网络数据，不通知上层脚本 */
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
    _conn_session_map.clear();
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
int32 lnetwork_mgr::set_cs_cmd( lua_State *L )
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
int32 lnetwork_mgr::set_ss_cmd( lua_State *L )
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
int32 lnetwork_mgr::set_sc_cmd( lua_State *L )
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
 * network_mgr:close( conn_id,false )
 */
int32 lnetwork_mgr::close( lua_State *L )
{
    uint32 conn_id = luaL_checkinteger( L,1 );
    bool flush = lua_toboolean( L,2 );

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
    _socket->stop( flush );

    /* 这里不能删除内存，因为脚本并不知道是否会再次访问此socket
     * 比如底层正在一个for循环里回调脚本时，就需要再次访问
     */
    _deleting.push_back( conn_id );

    return 0;
}

/* 监听端口
 * network_mgr:listen( host,port,conn_type )
 */
int32 lnetwork_mgr::listen( lua_State *L )
{
    const char *host = luaL_checkstring( L,1 );
    if ( !host )
    {
        return luaL_error( L,"host not specify" );
    }

    int32 port      = luaL_checkinteger( L,2 );
    int32 conn_type = luaL_checkinteger( L,3 );
    if ( conn_type <= socket::CNT_NONE || conn_type >= socket::CNT_MAX )
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
int32 lnetwork_mgr::connect( lua_State *L )
{
    const char *host = luaL_checkstring( L,1 );
    if ( !host )
    {
        return luaL_error( L,"host not specify" );
    }

    int32 port      = luaL_checkinteger( L,2 );
    int32 conn_type = luaL_checkinteger( L,3 );
    if ( conn_type <= socket::CNT_NONE || conn_type >= socket::CNT_MAX )
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
int32 lnetwork_mgr::load_one_schema( lua_State *L )
{
    int32 type = luaL_checkinteger( L,1 );
    const char *path = luaL_checkstring( L,2 );

    if ( type < codec::CDC_NONE || type >= codec::CDC_MAX ) return -1;

    int32 count = static_global::codec_mgr()->
        load_one_schema( static_cast<codec::codec_t>(type),path );

    lua_pushinteger( L,count );
    return 1;
}

/* 设置(客户端)连接所有者 */
int32 lnetwork_mgr::set_conn_owner( lua_State *L )
{
    uint32 conn_id = static_cast<uint32>( luaL_checkinteger( L,1) );
    owner_t owner  = luaL_checkinteger( L,2 );

    class socket *sk = get_conn_by_conn_id( conn_id );
    if ( !sk )
    {
        return luaL_error( L,"invalid connection" );
    }

    if ( socket::CNT_SCCN != sk->conn_type() )
    {
        return luaL_error( L,"illegal connection type" );
    }

    sk->set_object_id( owner );
    _owner_map[owner]  = conn_id;

    return 0;
}

/* 解除(客户端)连接所有者 */
int32 lnetwork_mgr::unset_conn_owner( lua_State *L )
{
    uint32 conn_id = static_cast<uint32>( luaL_checkinteger( L,1) );
    owner_t owner  = luaL_checkinteger( L,2 );

    _owner_map.erase( owner );

    // 这个时候可能socket已被销毁
    class socket *sk = get_conn_by_conn_id( conn_id );
    if ( sk ) sk->set_object_id( 0 );

    return 0;
}

/* 设置(服务器)连接session */
int32 lnetwork_mgr::set_conn_session( lua_State *L )
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
int32 lnetwork_mgr::set_curr_session( lua_State *L )
{
    _session = luaL_checkinteger( L,1 );
    return 0;
}

class packet *lnetwork_mgr::lua_check_packet( lua_State *L,socket::conn_t conn_ty )
{
    uint32 conn_id = static_cast<uint32>( luaL_checkinteger( L,1 ) );

    return raw_check_packet( L,conn_id,conn_ty );
}

class packet *lnetwork_mgr::raw_check_packet(
        lua_State *L,uint32 conn_id,socket::conn_t conn_ty )
{
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
int32 lnetwork_mgr::send_srv_packet( lua_State *L )
{
    class packet *pkt = lua_check_packet( L,socket::CNT_NONE );
    pkt->pack_srv( L,2 );

    return 0;
}


/* 发送s2c数据包
 * network_mgr:send_clt_packet( conn_id,cmd,errno,pkt )
 */
int32 lnetwork_mgr::send_clt_packet( lua_State *L )
{
    class packet *pkt = lua_check_packet( L,socket::CNT_NONE );
    pkt->pack_clt( L,2 );

    return 0;
}

/* 发送s2s数据包
 * network_mgr:send_s2s_packet( conn_id,cmd,errno,pkt )
 */
int32 lnetwork_mgr::send_s2s_packet( lua_State *L )
{
    class packet *pkt = lua_check_packet( L,socket::CNT_SSCN );

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
int32 lnetwork_mgr::send_ssc_packet( lua_State *L )
{
    class packet *pkt = lua_check_packet( L,socket::CNT_SSCN );

    // ssc数据包只有stream_packet能打包
    if ( packet::PKT_STREAM != pkt->type() )
    {
        return luaL_error( L,"illegal packet type" );
    }

    (reinterpret_cast<stream_packet *>(pkt))->pack_ssc( L,2 );

    return 0;
}

/* 获取http报文头数据 */
int32 lnetwork_mgr::get_http_header( lua_State *L )
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
int32 lnetwork_mgr::send_rpc_packet( lua_State *L )
{
    class packet *pkt = lua_check_packet( L,socket::CNT_SSCN );

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
int32 lnetwork_mgr::send_raw_packet( lua_State *L )
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

    sk->append( ctx ,size );

    return 0;
}

/* 设置发送缓冲区大小 */
int32 lnetwork_mgr::set_send_buffer_size( lua_State *L )
{
    uint32 conn_id  = luaL_checkinteger( L,1 );
    uint32 max      = luaL_checkinteger( L,2 );
    uint32 ctx_size = luaL_checkinteger( L,3 );
    int32  over_action = luaL_optinteger( L,4,socket::OAT_NONE );

    socket_map_t::iterator itr = _socket_map.find( conn_id );
    if ( itr == _socket_map.end() )
    {
        return luaL_error( L,"no such socket found" );
    }

    if ( over_action < socket::OAT_NONE || over_action >= socket::OAT_MAX )
    {
        return luaL_error( L,"overflow action value illegal" );
    }

    class socket *_socket = itr->second;
    _socket->set_send_size( 
        max,ctx_size,static_cast<socket::over_action_t>(over_action) );

    return 0;
}

/* 设置接收缓冲区大小 */
int32 lnetwork_mgr::set_recv_buffer_size( lua_State *L )
{
    uint32 conn_id  = luaL_checkinteger( L,1 );
    uint32 max      = luaL_checkinteger( L,2 );
    uint32 ctx_size = luaL_checkinteger( L,3 );

    socket_map_t::iterator itr = _socket_map.find( conn_id );
    if ( itr == _socket_map.end() )
    {
        return luaL_error( L,"no such socket found" );
    }

    class socket *_socket = itr->second;
    _socket->set_recv_size( max,ctx_size );

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

    /* 由网关转发给客户的数据包，是根据_socket_map来映射连接的。
     * 但是上层逻辑是脚本，万一发生错误则可能导致_socket_map的映射出现错误
     * 造成玩家数据发送到另一个玩家去了
     */
    socket *sk = itr->second;
    if ( sk->get_object_id() != owner )
    {
        ERROR("get_conn_by_owner not match:%d,conn = %d",owner,sk->conn_id());
        return NULL;
    }

    return sk;
}

/* 新增连接 */
bool lnetwork_mgr::accept_new( uint32 conn_id,class socket *new_sk )
{
    uint32 new_conn_id = new_sk->conn_id();

    _socket_map[new_conn_id] = new_sk;

    static lua_State *L = static_global::state();
    lua_pushcfunction( L,traceback );

    lua_getglobal( L,"conn_accept" );
    lua_pushinteger( L,conn_id );
    lua_pushinteger( L,new_conn_id );

    if ( expect_false( LUA_OK != lua_pcall( L,2,0,1 ) ) )
    {
        /* 出错后，无法得知脚本能否继续处理此连接
         * 为了防止死链，这里直接删除此连接
         */
        _deleting.push_back( new_conn_id );
        ERROR( "accept new socket:%s",lua_tostring( L,-1 ) );

        lua_pop( L,2 ); /* remove traceback and error object */
        return   false;
    }
    lua_pop( L,1 ); /* remove traceback */

    return true;
}

/* 连接回调 */
bool lnetwork_mgr::connect_new( uint32 conn_id,int32 ecode )
{
    static lua_State *L = static_global::state();
    lua_pushcfunction( L,traceback );

    lua_getglobal( L,"conn_new" );
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
bool lnetwork_mgr::connect_del( uint32 conn_id )
{
    _deleting.push_back( conn_id );

    static lua_State *L = static_global::state();
    lua_pushcfunction( L,traceback );

    lua_getglobal( L,"conn_del" );
    lua_pushinteger( L,conn_id );

    if ( expect_false( LUA_OK != lua_pcall( L,1,0,1 ) ) )
    {
        ERROR( "conn_del:%s",lua_tostring( L,-1 ) );

        lua_pop( L,2 ); /* remove traceback and error object */
        return   false;
    }
    lua_pop( L,1 ); /* remove traceback */

    return true;
}

/* 设置socket的io方式
 * network_mgr:set_conn_io( conn_id,io_type[,io_ctx] )
 */
int32 lnetwork_mgr::set_conn_io( lua_State *L )
{
    uint32 conn_id = luaL_checkinteger( L,1 );
    int32 io_type  = luaL_checkinteger( L,2 );
    int32 io_ctx   = luaL_optinteger  ( L,3,0 );

    class socket *sk = get_conn_by_conn_id( conn_id );
    if ( !sk )
    {
        return luaL_error( L,"invalid conn id" );
    }

    if ( io_type < io::IOT_NONE || io_type >= io::IOT_MAX )
    {
        return luaL_error( L,"invalid io type" );
    }

    if ( sk->set_io( static_cast<io::io_t>(io_type),io_ctx ) < 0 )
    {
        return luaL_error( L,"set conn io error" );
    }

    return 0;
}

int32 lnetwork_mgr::set_conn_codec( lua_State *L ) /* 设置socket的编译方式 */
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

    if ( sk->set_codec_type( static_cast<codec::codec_t>( codec_type ) ) < 0 )
    {
        return luaL_error( L,"set conn codec error" );
    }

    return 0;
}

int32 lnetwork_mgr::set_conn_packet( lua_State *L ) /* 设置socket的打包方式 */
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

    if ( sk->set_packet( static_cast<packet::packet_t>( packet_type ) ) < 0 )
    {
        return luaL_error( L,"set conn packet error" );
    }
    return 0;
}

int32 lnetwork_mgr::new_ssl_ctx( lua_State *L ) /* 创建一个ssl上下文 */
{
    int32 sslv = luaL_checkinteger( L,1 );
    const char *cert_file = lua_tostring( L,2 );

    int32 keyt = luaL_optinteger( L,3,0 );
    const char *key_file = lua_tostring( L,4 );
    const char *passwd   = lua_tostring( L,5 );

    if ( sslv <= ssl_mgr::SSLV_NONE || sslv >= ssl_mgr::SSLV_MAX )
    {
        return luaL_error( L,"invalid ssl version" );
    }

    if ( keyt < ssl_mgr::KEYT_NONE || keyt >= ssl_mgr::KEYT_MAX )
    {
        return luaL_error( L,"invalid ssl key type" );
    }

    int32 idx = static_global::ssl_mgr()->new_ssl_ctx(
        static_cast<ssl_mgr::sslv_t>(sslv),cert_file,
        static_cast<ssl_mgr::key_t>(keyt),key_file,passwd );

    if ( idx < 0 )
    {
        return luaL_error( L,"new ssl ctx error" );
    }

    lua_pushinteger( L,idx );
    return 1;
}

/* 把客户端数据包转发给另一服务器
 * 这个函数如果返回false，则会将协议在当前进程派发
 */
bool lnetwork_mgr::cs_dispatch(
    int32 cmd,const class socket *src_sk,const char *ctx,size_t size ) const
{
    const cmd_cfg_t *cmd_cfg = get_cs_cmd( cmd );
    if ( expect_false(!cmd_cfg) )
    {
        ERROR( "cs_dispatch cmd(%d) no cmd cfg found",cmd );
        return false;
    }

    if ( cmd_cfg->_session == _session ) return false;

    int32 conn_id = src_sk->conn_id();
    /* 这个socket必须经过认证，归属某个对象后才能转发到其他服务器
     * 防止网关后面的服务器被攻击
     */
    int64 object_id = src_sk->get_object_id();
    if ( expect_false(!object_id) )
    {
        ERROR( "cs_dispatch cmd(%d) socket do NOT have object:%d",cmd,conn_id );
        return false;
    }

    /* 这个指令不是在当前进程处理，自动转发到对应进程 */
    class socket *dest_sk  = get_conn_by_session( cmd_cfg->_session );
    if ( !dest_sk )
    {
        ERROR( "client packet forwarding no destination found.cmd:%d",cmd );
        return true; /* 如果转发失败，也相当于转发了 */
    }

    codec::codec_t codec_ty = src_sk->get_codec_type();

    struct s2s_header s2sh;
    SET_HEADER_LENGTH( s2sh, size, cmd, SET_LENGTH_FAIL_BOOL );
    s2sh._cmd    = cmd;
    s2sh._packet = SPKT_CSPK;
    s2sh._codec  = codec_ty;
    s2sh._errno  = 0;
    s2sh._owner  = static_cast<owner_t>(object_id);

    class buffer &send = dest_sk->send_buffer();
    send.append( &s2sh,sizeof(struct s2s_header) );
    send.append( ctx,size );

    dest_sk->pending_send();
    return true;
}

/* 发送ping-pong等数据包 */
int32 lnetwork_mgr::send_ctrl_packet ( lua_State *L )
{
    class packet *pkt = lua_check_packet( L,socket::CNT_NONE );

    // 检测packet类型，基类是websocket_packet才能发送控制帧
    packet::packet_t type = pkt->type();
    if ( (packet::PKT_WSSTREAM != type) && (packet::PKT_WEBSOCKET != type) )
    {
        return luaL_error( L,"illegal packet type" );
    }

    (reinterpret_cast<class websocket_packet *>(pkt))->pack_ctrl( L,2 );

    return 0;
}

/* 广播到所有连接到当前进程的服务器
 * srv_multicast( conn_list,codec_type,cmd,errno,pkt )
 */
int32 lnetwork_mgr::srv_multicast( lua_State *L )
{
    if ( !lua_istable( L,1 ) )
    {
        return luaL_error( L,
            "expect table,got %s",lua_typename( L,lua_type(L,1) ) );
    }
    int32 codec_ty = luaL_checkinteger( L,2 );
    int32 cmd      = luaL_checkinteger( L,3 );
    int32 ecode    = luaL_checkinteger( L,4 );
    if ( codec_ty < codec::CDC_NONE || codec_ty >= codec::CDC_MAX )
    {
        return luaL_error( L,"illegal codec type" );
    }

    if ( !lua_istable( L,5 ) )
    {
        return luaL_error( L,
            "expect table,got %s",lua_typename( L,lua_type(L,5) ) );
    }

    const cmd_cfg_t *cfg = get_ss_cmd( cmd );
    if ( !cfg )
    {
        return luaL_error( L,"no command conf found: %d",cmd );
    }

    codec *encoder = static_global::codec_mgr()
        ->get_codec( static_cast<codec::codec_t>(codec_ty) );
    if ( !cfg )
    {
        return luaL_error( L,"no codec conf found: %d",cmd );
    }

    const char *buffer = NULL;
    int32 len = encoder->encode( L,5,&buffer,cfg );
    if ( len < 0 )
    {
        encoder->finalize();
        ERROR( "srv_multicast encode error" );
        return 0;
    }

    if ( len > MAX_PACKET_LEN )
    {
        encoder->finalize();
        return luaL_error( L,"buffer size over MAX_PACKET_LEN" );
    }

    lua_pushnil(L);  /* first key */
    while ( lua_next(L, 1) != 0 )
    {
        if ( !lua_isinteger( L,-1 ) )
        {
            lua_pop( L, 1 );
            encoder->finalize();
            return luaL_error( L,"conn list expect integer" );
        }

        uint32 conn_id = static_cast<uint32>( lua_tointeger(L,-1) );

        lua_pop( L, 1 );
        class packet *pkt = raw_check_packet( L,conn_id,socket::CNT_SSCN );
        if ( !pkt )
        {
            ERROR( "srv_multicast conn not found:%ud",conn_id );
            continue;
        }

        if ( pkt->raw_pack_ss( cmd,ecode,_session,buffer,len ) < 0 )
        {
            ERROR( "srv_multicast can not raw_pack_ss:%ud",conn_id );
            continue;
        }
    }

    encoder->finalize();
    return 0;
}

/* 网关进程广播数据到客户端
 * clt_multicast( conn_list,codec_type,cmd,errno,pkt )
 */
int32 lnetwork_mgr::clt_multicast( lua_State *L )
{
    if ( !lua_istable( L,1 ) )
    {
        return luaL_error( L,
            "expect table,got %s",lua_typename( L,lua_type(L,1) ) );
    }
    int32 codec_ty = luaL_checkinteger( L,2 );
    int32 cmd      = luaL_checkinteger( L,3 );
    int32 ecode    = luaL_checkinteger( L,4 );
    if ( codec_ty < codec::CDC_NONE || codec_ty >= codec::CDC_MAX )
    {
        return luaL_error( L,"illegal codec type" );
    }

    if ( !lua_istable( L,5 ) )
    {
        return luaL_error( L,
            "expect table,got %s",lua_typename( L,lua_type(L,5) ) );
    }

    const cmd_cfg_t *cfg = get_sc_cmd( cmd );
    if ( !cfg )
    {
        return luaL_error( L,"no command conf found: %d",cmd );
    }

    codec *encoder = static_global::codec_mgr()
        ->get_codec( static_cast<codec::codec_t>(codec_ty) );
    if ( !cfg )
    {
        return luaL_error( L,"no codec conf found: %d",cmd );
    }

    const char *buffer = NULL;
    int32 len = encoder->encode( L,5,&buffer,cfg );
    if ( len < 0 )
    {
        encoder->finalize();
        ERROR( "clt_multicast encode error" );
        return 0;
    }

    if ( len > MAX_PACKET_LEN )
    {
        encoder->finalize();
        return luaL_error( L,"buffer size over MAX_PACKET_LEN" );
    }

    lua_pushnil(L);  /* first key */
    while ( lua_next(L, 1) != 0 )
    {
        if ( !lua_isinteger( L,-1 ) )
        {
            lua_pop( L, 1 );
            encoder->finalize();
            return luaL_error( L,"conn list expect integer" );
        }

        uint32 conn_id = static_cast<uint32>( lua_tointeger(L,-1) );

        lua_pop( L, 1 );
        class packet *pkt = raw_check_packet( L,conn_id,socket::CNT_SSCN );
        if ( !pkt )
        {
            ERROR( "clt_multicast conn not found:%ud",conn_id );
            continue;
        }

        if ( pkt->raw_pack_clt( cmd,ecode,buffer,len ) < 0 )
        {
            ERROR( "clt_multicast can not raw_pack_ss:%ud",conn_id );
            continue;
        }
    }

    encoder->finalize();
    return 0;
}

/* 非网关数据广播数据到客户端
 * ssc_multicast( conn_id,mask,conn_list or args_list,codec_type,cmd,errno,pkt )
 */
int32 lnetwork_mgr::ssc_multicast( lua_State *L )
{
    class packet *pkt = lua_check_packet( L,socket::CNT_SSCN );

    // ssc广播数据包只有stream_packet能打包
    if ( packet::PKT_STREAM != pkt->type() )
    {
        return luaL_error( L,"illegal packet type" );
    }

    (reinterpret_cast<stream_packet *>(pkt))->pack_ssc_multicast( L,2 );

    return 0;
}
