#include "lnetwork_mgr.h"

#include "ltools.h"
#include "lstate.h"
#include "../net/packet.h"
#include "../net/stream_socket.h"

class lnetwork_mgr *lnetwork_mgr::_network_mgr = NULL;

void lnetwork_mgr::uninstance()
{
    if ( _network_mgr )
    {
        delete _network_mgr;
        _network_mgr = NULL;
    }
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

/* 设置某个指令的参数
 * network_mgr:set_cmd( cmd,schema,object[,mask,session] )
 */
int32 lnetwork_mgr::set_cmd()
{
    int32 cmd          = luaL_checkinteger( L,1 );
    const char *schema = luaL_checkstring ( L,2 );
    const char *object = luaL_checkstring ( L,3 );
    int32 mask         = luaL_optinteger  ( L,4,0 );
    int32 session      = luaL_optinteger  ( L,5,_session );

    cmd_cfg_t &cfg = _cmd_map[cmd];
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
        luaL_error( L,strerror(errno) );
        return 0;
    }

    _socket_map[conn_id] = _socket;

    lua_pushinteger( L,conn_id );
    return 1;
}

/* 新增连接 */
bool lnetwork_mgr::accept_new( uint32 conn_id,class socket *new_sk )
{
    lua_pushcfunction( L,traceback );

    lua_getglobal( L,"socket_accpet_new" );
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

    lua_pushinteger( L,conn_id );
    return 1;
}

/* 连接回调 */
bool lnetwork_mgr::connect_new( uint32 conn_id,int32 ecode )
{
    lua_pushcfunction( L,traceback );

    lua_getglobal( L,"socket_connect" );
    lua_pushinteger( L,conn_id );
    lua_pushinteger( L,ecode   );

    if ( expect_false( LUA_OK != lua_pcall( L,2,0,1 ) ) )
    {
        /* 出错后，无法得知脚本能否继续处理此连接
         * 为了防止死链，这里直接删除此连接
         */
        _deleting.push_back( conn_id );
        ERROR( "connect_cb:%s",lua_tostring( L,-1 ) );

        lua_pop( L,1 ); /* remove traceback and error object */
        return   false;
    }
    lua_pop( L,1 ); /* remove traceback */

    return true;
}

/* 获取指令配置 */
const cmd_cfg_t *lnetwork_mgr::get_cmd_cfg( int32 cmd )
{
    cmd_map_t::iterator itr = _cmd_map.find( cmd );
    if ( itr == _cmd_map.end() ) return NULL;

    return &(itr->second);
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

/* 加载schema文件 */
int32 lnetwork_mgr::load_schema()
{
    const char *path = luaL_checkstring( L,1 );

    int32 count = packet::instance()->load_schema( path );

    lua_pushinteger( L,count );
    return 1;
}