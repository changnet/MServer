#include "lnetwork_mgr.h"

#include "lstate.h"
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
    :L(L),_conn_seed(0),_deletecnt(0)
{
    assert( "lnetwork_mgr is singleton",NULL == _network_mgr );
}

/* 产生一个唯一的连接id 
 * 之所以不用系统的文件描述符fd，是因为fd对于上层逻辑不可控。比如一个fd被释放，可能在多个进程
 * 之间还未处理完，此fd就被重用了。当前的连接id不太可能会在短时间内重用。
 */
uint32 lnetwork_mgr::connect_id()
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
    _deletecnt ++;

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

    uint32 conn_id = connect_id();
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


// int32 lsocket::connect()
// {
//     if ( socket::active() )
//     {
//         return luaL_error( L,"connect:socket already active");
//     }

//     const char *host = luaL_checkstring( L,1 );
//     if ( !host )
//     {
//         return luaL_error( L,"connect:host not specify" );
//     }

//     int32 port = luaL_checkinteger( L,2 );

//     int32 fd = socket::connect( host,port );
//     if ( fd < 0 )
//     {
//         luaL_error( L,strerror(errno) );
//         return 0;
//     }

//     socket::set<lsocket,&lsocket::connect_cb>( this );
//     socket::start( fd,EV_WRITE ); /* write事件 */

//     lua_pushinteger( L,fd );
//     return 1;
// }

// /* 发送原始数据，二进制或字符串 */
// int32 lsocket::send()
// {
//     if ( !socket::active() )
//     {
//         return luaL_error( L,"socket::send not valid");
//     }

//     size_t len = 0;
//     const char *sz = luaL_checklstring( L,1,&len );
//     /* 允许指定发送的长度 */
//     len = luaL_optinteger( L,2,len );

//     if ( !sz || len <= 0 )
//     {
//         return luaL_error( L,"socket::send nothing to send" );
//     }

//     if ( !socket::append( sz,len ) )
//     {
//         return luaL_error( L,"socket::send out of buffer" );
//     }

//     return 0;
// }

// void lsocket::message_cb( int32 revents )
// {
//     assert( "libev read cb error",!(EV_ERROR & revents) );

//     /* 就游戏中的绝大多数消息而言，一次recv就能接收完成，不需要while接收直到出错。而且
//      * 当前设定的缓冲区与socket一致(8192)，socket缓冲区几乎不可能还有数据，不需要多调用
//      * 一次recv。退一步，假如还有数据，epoll当前为LT模式，下一次回调再次读取
//      * 如果启用while,需要检测_socket在lua层逻辑中是否被关闭，防止自杀的情况
//      */

//     int32 ret = socket::recv();

//     /* disconnect or error */
//     if ( 0 == ret )
//     {
//         on_disconnect();
//         return;
//     }
//     else if ( ret < 0 )
//     {
//         if ( EAGAIN != errno && EWOULDBLOCK != errno )
//         {
//             ERROR( "socket recv error(maybe out of buffer):%s\n",strerror(errno) );
//             on_disconnect();
//         }
//         return;
//     }

//     /* 此框架中，socket的内存由lua管理，无法预知lua会何时释放内存
//      * 因此不要在C++层用while去解析协议
//      */
//     int32 complete = is_message_complete();
//     if ( complete < 0 )  //error
//     {
//         on_disconnect();
//         return;
//     }

//     if ( 0 == complete ) return;

//     lua_pushcfunction(L,traceback);
//     lua_rawgeti(L, LUA_REGISTRYINDEX, ref_message);
//     lua_rawgeti(L, LUA_REGISTRYINDEX, ref_self);
//     if ( expect_false( LUA_OK != lua_pcall(L,1,0,-3) ) )
//     {
//         ERROR( "message callback fail:%s\n",lua_tostring(L,-1) );

//         lua_pop(L,2); /* remove error message and traceback */
//         return;
//     }

//     lua_pop(L,1); /* remove traceback */
// }

// void lsocket::on_disconnect()
// {
//     /* close fd first,in case reconnect in lua callback */
//     socket::stop();

//     lua_pushcfunction(L,traceback);
//     lua_rawgeti(L, LUA_REGISTRYINDEX, ref_disconnect);
//     lua_rawgeti(L, LUA_REGISTRYINDEX, ref_self);
//     if ( expect_false( LUA_OK != lua_pcall(L,1,0,-3) ) )
//     {
//         ERROR( "socket disconect,call lua fail:%s\n",lua_tostring(L,-1) );
//         // DO NOT RETURN
//         lua_pop(L,1); /* remove error message */
//     }

//     lua_pop(L,1); /* remove traceback */
// }

// int32 lsocket::address()
// {
//     if ( !socket::active() )
//     {
//         return luaL_error( L,"socket::address not a active socket" );
//     }

//     lua_pushstring( L,socket::address() );
//     return 1;
// }