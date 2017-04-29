#include <lua.hpp>
#include <lparson.h>
#include <lrapidxml.hpp>
#include <lflatbuffers.hpp>

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
    LUA_LIB_OPEN("lua_flatbuffers",luaopen_lua_flatbuffers);
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */

    /* ============================对象方式调用================================ */
    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
    luaopen_ev    (L);
    luaopen_sql   (L);
    luaopen_log   (L);
    luaopen_timer (L);
    luaopen_acism (L);
    luaopen_mongo (L);
    // luaopen_http_socket(L);
    luaopen_network_mgr(L);
    // luaopen_stream_socket(L);
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
    lc.def<&leventloop::set_signal_ref>("set_signal_ref");

    return 0;
}

/*
int32 luaopen_http_socket( lua_State *L )
{
    lclass<lhttp_socket> lc(L,"Http_socket");
    lc.def<&lhttp_socket::send>("send");
    lc.def<&lhttp_socket::kill>("kill");
    lc.def<&lhttp_socket::listen> ("listen" );
    lc.def<&lhttp_socket::address>("address");
    lc.def<&lhttp_socket::connect>("connect");
    lc.def<&lhttp_socket::set_self_ref>     ("set_self_ref"     );
    lc.def<&lhttp_socket::set_on_command>   ("set_on_command"   );
    lc.def<&lhttp_socket::buffer_setting>   ("buffer_setting"   );
    lc.def<&lhttp_socket::set_on_acception> ("set_on_acception" );
    lc.def<&lhttp_socket::set_on_connection>("set_on_connection");
    lc.def<&lhttp_socket::set_on_disconnect>("set_on_disconnect");
    lc.def<&lhttp_socket::file_description> ("file_description" );

    lc.def<&lhttp_socket::next>   ("next");
    lc.def<&lhttp_socket::get_url>   ("get_url");
    lc.def<&lhttp_socket::get_body>  ("get_body" );
    lc.def<&lhttp_socket::get_method>("get_method");
    lc.def<&lhttp_socket::get_status>("get_status");
    lc.def<&lhttp_socket::is_upgrade>   ("is_upgrade");
    lc.def<&lhttp_socket::get_head_field>("get_head_field");

    return 0;
}
*/
int32 luaopen_timer ( lua_State *L )
{
    lclass<ltimer> lc(L,"Timer");
    lc.def<&ltimer::start> ( "start"  );
    lc.def<&ltimer::stop>  ( "stop"   );
    lc.def<&ltimer::active>( "active" );
    lc.def<&ltimer::set_self>( "set_self" );
    lc.def<&ltimer::set_callback>( "set_callback" );

    return 0;
}

int32 luaopen_sql( lua_State *L )
{
    lclass<lsql> lc(L,"Sql");
    lc.def<&lsql::start> ( "start" );
    lc.def<&lsql::stop>  ( "stop"  );

    lc.def<&lsql::do_sql>         ( "do_sql"         );
    lc.def<&lsql::next_result>    ( "next_result"    );
    lc.def<&lsql::self_callback > ( "self_callback"  );
    lc.def<&lsql::read_callback > ( "read_callback"  );
    lc.def<&lsql::error_callback> ( "error_callback" );

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
    lc.def<&lmongo::next_result>     ( "next_result"     );
    lc.def<&lmongo::self_callback >  ( "self_callback"   );
    lc.def<&lmongo::read_callback >  ( "read_callback"   );
    lc.def<&lmongo::error_callback>  ( "error_callback"  );

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

/*
int32 luaopen_stream_socket( lua_State *L )
{
    lclass<lstream_socket> lc(L,"Stream_socket");
    lc.def<&lstream_socket::send>("send");
    lc.def<&lstream_socket::kill>("kill");
    lc.def<&lstream_socket::listen> ("listen" );
    lc.def<&lstream_socket::address>("address");
    lc.def<&lstream_socket::connect>("connect");
    lc.def<&lstream_socket::srv_next>("srv_next");
    lc.def<&lstream_socket::clt_next>("clt_next");
    lc.def<&lstream_socket::css_cmd >("css_cmd" );
    lc.def<&lstream_socket::rpc_send >("rpc_send" );
    lc.def<&lstream_socket::scmd_next>("scmd_next");
    lc.def<&lstream_socket::rpc_decode >("rpc_decode" );
    lc.def<&lstream_socket::set_self_ref>     ("set_self_ref"     );
    lc.def<&lstream_socket::set_on_command>   ("set_on_command"   );
    lc.def<&lstream_socket::buffer_setting>   ("buffer_setting"   );
    lc.def<&lstream_socket::set_on_acception> ("set_on_acception" );
    lc.def<&lstream_socket::set_on_connection>("set_on_connection");
    lc.def<&lstream_socket::set_on_disconnect>("set_on_disconnect");
    lc.def<&lstream_socket::file_description> ("file_description" );

    lc.def<&lstream_socket::ss_flatbuffers_send   > ("ss_flatbuffers_send"   );
    lc.def<&lstream_socket::ss_flatbuffers_decode > ("ss_flatbuffers_decode" );
    lc.def<&lstream_socket::sc_flatbuffers_send   > ("sc_flatbuffers_send"   );
    lc.def<&lstream_socket::sc_flatbuffers_decode > ("sc_flatbuffers_decode" );
    lc.def<&lstream_socket::ssc_flatbuffers_send  > ("ssc_flatbuffers_send"  );
    lc.def<&lstream_socket::ssc_flatbuffers_decode> ("ssc_flatbuffers_decode");
    lc.def<&lstream_socket::cs_flatbuffers_decode > ("cs_flatbuffers_decode" );
    lc.def<&lstream_socket::cs_flatbuffers_send   > ("cs_flatbuffers_send"   );
    lc.def<&lstream_socket::css_flatbuffers_send  > ("css_flatbuffers_send"  );
    lc.def<&lstream_socket::css_flatbuffers_decode> ("css_flatbuffers_decode");

    return 0;
}
*/
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

    lc.def<&lnetwork_mgr::set_cmd> ( "set_cmd" );
    lc.def<&lnetwork_mgr::listen> ( "listen" );
    lc.def<&lnetwork_mgr::connect> ( "connect" );
    lc.def<&lnetwork_mgr::load_schema> ( "load_schema" );
    lc.def<&lnetwork_mgr::send_c2s_packet> ( "send_c2s_packet" );
    lc.def<&lnetwork_mgr::send_s2c_packet> ( "send_s2c_packet" );

    return 0;
}
