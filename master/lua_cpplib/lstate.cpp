#include <lua.hpp>
#include <lparson.h>
#include <lrapidxml.hpp>

#include "llog.h"
#include "lsql.h"
#include "lutil.h"
#include "lmongo.h"
#include "lstate.h"
#include "lclass.h"
#include "ltimer.h"
#include "lacism.h"
#include "leventloop.h"
#include "lobj_counter.h"
#include "lnetwork_mgr.h"

#include "../net/socket.h"

#define LUA_LIB_OPEN( name,func ) \
    do{luaL_requiref(L, name, func, 1);lua_pop(L, 1);  /* remove lib */}while(0)

class lstate *lstate::_state = NULL;
class lstate *lstate::instance()
{
    if ( !_state )
    {
        _state = new lstate();
    }

    return _state;
}

void lstate::uninstance()
{
    if ( _state ) delete _state;
    _state = NULL;
}

lstate::lstate()
{
    /* 初始化lua */
    L = luaL_newstate();
    if ( !L )
    {
        ERROR( "lua new state fail\n" );
        exit( 1 );
    }
    luaL_openlibs(L);
    open_cpp();
}

lstate::~lstate()
{
    assert( "lua stack not clean at program exit",0 == lua_gettop(L) );

    /* Destroys all objects in the given Lua state (calling the corresponding
     * garbage-collection metamethods, if any) and frees all dynamic memory used
     * by this state
     */
    lua_close(L);
    L = NULL;
}

int32 luaopen_ev    ( lua_State *L );
int32 luaopen_sql   ( lua_State *L );
int32 luaopen_log   ( lua_State *L );
int32 luaopen_timer ( lua_State *L );
int32 luaopen_acism ( lua_State *L );
int32 luaopen_mongo ( lua_State *L );
int32 luaopen_network_mgr( lua_State *L );

void lstate::set_lua_path()
{
    /* 得到绝对工作路径,getcwd无法获取自启动程序的正确工作路径 */
    char cwd[PATH_MAX] = {0};
    if ( getcwd( cwd,PATH_MAX ) <= 0 )
    {
        ERROR( "get current working directory fail\n" );
        exit( 1 );
    }

    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    const char *old_path = lua_tostring(L, -1);

    char new_path[PATH_MAX] = {0};
    if ( snprintf( new_path,PATH_MAX,"%s;%s/lua_script/?.lua",old_path,cwd ) >= PATH_MAX )
    {
        ERROR( "lua init,lua path overflow\n" );
        lua_close( L );
        exit( 1 );
    }

    lua_pop(L, 1);    /* drop old path field */
    lua_pushstring(L, new_path);
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);   /* drop package table */
}

void lstate::set_c_path()
{
    char cwd[PATH_MAX] = {0};
    if ( getcwd( cwd,PATH_MAX ) <= 0 )
    {
        ERROR( "get current working directory fail\n" );
        exit( 1 );
    }

    lua_getglobal(L, "package");
    lua_getfield(L, -1, "cpath");
    const char *old_path = lua_tostring(L, -1);

    char new_path[PATH_MAX] = {0};
    if ( snprintf( new_path,PATH_MAX,"%s;%s/c_module/?.so",old_path,cwd ) >= PATH_MAX )
    {
        ERROR( "lua init,c path overflow\n" );
        lua_close( L );
        exit( 1 );
    }

    lua_pop(L, 1);    /* drop old path field */
    lua_pushstring(L, new_path);
    lua_setfield(L, -2, "cpath");
    lua_pop(L, 1);   /* drop package table */
}

void lstate::open_cpp()
{
    /* 把当前工作目录加到lua的path */
    set_c_path();
    set_lua_path();

    /* ============================库方式调用================================== */
    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
    LUA_LIB_OPEN("util", luaopen_util);
    LUA_LIB_OPEN("lua_parson", luaopen_lua_parson);
    LUA_LIB_OPEN("lua_rapidxml", luaopen_lua_rapidxml);
    LUA_LIB_OPEN("obj_counter", luaopen_obj_counter);
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */

    /* ============================对象方式调用================================ */
    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
    luaopen_ev    (L);
    luaopen_sql   (L);
    luaopen_log   (L);
    luaopen_timer (L);
    luaopen_acism (L);
    luaopen_mongo (L);
    luaopen_network_mgr(L);
    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */

    /* when debug,make sure lua stack clean after init */
    assert( "lua stack not clean after init", 0 == lua_gettop(L) );
}

int32 luaopen_ev( lua_State *L )
{
    lclass<leventloop> lc(L,"Eventloop");
    lc.def<&leventloop::time>   ("time"   );
    lc.def<&leventloop::exit>   ("exit"   );
    lc.def<&leventloop::signal> ("signal" );
    lc.def<&leventloop::backend>("backend");

    return 0;
}

int32 luaopen_timer ( lua_State *L )
{
    lclass<ltimer> lc(L,"Timer");
    lc.def<&ltimer::set>   ( "set"    );
    lc.def<&ltimer::stop>  ( "stop"   );
    lc.def<&ltimer::start> ( "start"  );
    lc.def<&ltimer::active>( "active" );

    return 0;
}

int32 luaopen_sql( lua_State *L )
{
    lclass<lsql> lc(L,"Sql");
    lc.def<&lsql::start> ( "start" );
    lc.def<&lsql::stop>  ( "stop"  );

    lc.def<&lsql::do_sql> ( "do_sql" );

    return 0;
}

int32 luaopen_mongo( lua_State *L )
{
    lclass<lmongo> lc(L,"Mongo");
    lc.def<&lmongo::start> ( "start" );
    lc.def<&lmongo::stop>  ( "stop"  );

    lc.def<&lmongo::count>           ( "count"           );
    lc.def<&lmongo::find>            ( "find"            );
    lc.def<&lmongo::insert>          ( "insert"          );
    lc.def<&lmongo::update>          ( "update"          );
    lc.def<&lmongo::remove>          ( "remove"          );
    lc.def<&lmongo::find_and_modify> ( "find_and_modify" );

    return 0;
}

int32 luaopen_log( lua_State *L )
{
    lclass<llog> lc(L,"Log");
    lc.def<&llog::stop>  ("stop");
    lc.def<&llog::start> ("start");
    lc.def<&llog::write> ("write");
    lc.def<&llog::mkdir_p> ("mkdir_p");

    return 0;
}

int32 luaopen_acism( lua_State *L )
{
    lclass<lacism> lc(L,"Acism");

    lc.def<&lacism::scan> ( "scan" );
    lc.def<&lacism::replace> ( "replace" );
    lc.def<&lacism::load_from_file> ( "load_from_file" );

    return 0;
}

int32 luaopen_network_mgr( lua_State *L )
{
    lclass<lnetwork_mgr> lc(L,"Network_mgr");

    lc.def<&lnetwork_mgr::close> ( "close" );
    lc.def<&lnetwork_mgr::listen> ( "listen" );
    lc.def<&lnetwork_mgr::connect> ( "connect" );
    lc.def<&lnetwork_mgr::load_one_schema> ( "load_one_schema" );
    lc.def<&lnetwork_mgr::set_curr_session> ( "set_curr_session" );

    lc.def<&lnetwork_mgr::set_conn_session> ( "set_conn_session" );
    lc.def<&lnetwork_mgr::set_conn_owner>   ( "set_conn_owner"   );

    lc.def<&lnetwork_mgr::set_cs_cmd> ( "set_cs_cmd" );
    lc.def<&lnetwork_mgr::set_ss_cmd> ( "set_ss_cmd" );
    lc.def<&lnetwork_mgr::set_sc_cmd> ( "set_sc_cmd" );

    lc.def<&lnetwork_mgr::set_conn_io>     ( "set_conn_io"     );
    lc.def<&lnetwork_mgr::set_conn_codec>  ( "set_conn_codec"  );
    lc.def<&lnetwork_mgr::set_conn_packet> ( "set_conn_packet" );

    lc.def<&lnetwork_mgr::get_http_header> ( "get_http_header" );

    lc.def<&lnetwork_mgr::send_srv_packet > ( "send_srv_packet"  );
    lc.def<&lnetwork_mgr::send_clt_packet > ( "send_clt_packet"  );
    lc.def<&lnetwork_mgr::send_s2s_packet > ( "send_s2s_packet"  );
    lc.def<&lnetwork_mgr::send_ssc_packet > ( "send_ssc_packet"  );
    lc.def<&lnetwork_mgr::send_rpc_packet > ( "send_rpc_packet"  );
    lc.def<&lnetwork_mgr::send_raw_packet > ( "send_raw_packet"  );

    lc.def<&lnetwork_mgr::set_send_buffer_size> ( "set_send_buffer_size" );
    lc.def<&lnetwork_mgr::set_recv_buffer_size> ( "set_recv_buffer_size" );

    lc.def<&lnetwork_mgr::new_ssl_ctx> ( "new_ssl_ctx" );

    lc.set( "CNT_NONE",socket::CNT_NONE );
    lc.set( "CNT_CSCN",socket::CNT_CSCN );
    lc.set( "CNT_SCCN",socket::CNT_SCCN );
    lc.set( "CNT_SSCN",socket::CNT_SSCN );
    lc.set( "CNT_HTTP",socket::CNT_HTTP );
    lc.set( "CNT_WEBS",socket::CNT_WEBS );

    lc.set( "IOT_NONE",io::IOT_NONE );
    lc.set( "IOT_SSL" ,io::IOT_SSL  );

    lc.set( "PKT_NONE"     ,packet::PKT_NONE      );
    lc.set( "PKT_HTTP"     ,packet::PKT_HTTP      );
    lc.set( "PKT_STREAM"   ,packet::PKT_STREAM    );
    lc.set( "PKT_WEBSOCKET",packet::PKT_WEBSOCKET );

    lc.set( "CDC_NONE"    ,codec::CDC_NONE    );
    lc.set( "CDC_BSON"    ,codec::CDC_BSON    );
    lc.set( "CDC_STREAM"  ,codec::CDC_STREAM  );
    lc.set( "CDC_FLATBUF" ,codec::CDC_FLATBUF );
    lc.set( "CDC_PROTOBUF",codec::CDC_PROTOBUF);

    return 0;
}
