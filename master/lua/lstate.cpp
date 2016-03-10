#include <lua.hpp>
#include <lparson.h>

#include "llog.h"
#include "lsql.h"
#include "lutil.h"
#include "lmongo.h"
#include "lstate.h"
#include "lclass.h"
#include "ltimer.h"
#include "lhttp_socket.h"
#include "leventloop.h"

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
int32 luaopen_mongo ( lua_State *L );
int32 luaopen_http_socket( lua_State *L );

void lstate::set_lua_path()
{
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

    luaL_requiref(L, "util", luaopen_util, 1);
    lua_pop(L, 1);  /* remove lib */

    luaL_requiref(L, "lua_parson", luaopen_lua_parson, 1);
    lua_pop(L, 1);  /* remove lib */

    luaopen_ev    (L);
    luaopen_sql   (L);
    luaopen_log   (L);
    luaopen_timer (L);
    luaopen_mongo (L);
    luaopen_http_socket(L);

    /* when debug,make sure lua stack clean after init */
    assert( "lua stack not clean after init", 0 == lua_gettop(L) );
}

int32 luaopen_ev( lua_State *L )
{
    lclass<leventloop> lc(L,"Eventloop");
    lc.def<&leventloop::run>   ("run");
    lc.def<&leventloop::time>  ("time");
    lc.def<&leventloop::exit>  ("exit");
    lc.def<&leventloop::signal>("signal");
    lc.def<&leventloop::set_signal_ref>("set_signal_ref");

    return 0;
}

int32 luaopen_http_socket( lua_State *L )
{
    lclass<lhttp_socket> lc(L,"Http_socket");
    lc.def<&lhttp_socket::send>("send");
    lc.def<&lhttp_socket::kill>("kill");
    lc.def<&lhttp_socket::listen> ("listen" );
    lc.def<&lhttp_socket::address>("address");
    lc.def<&lhttp_socket::connect>("connect");
    lc.def<&lhttp_socket::set_self_ref>     ("set_self_ref"     );
    lc.def<&lhttp_socket::set_on_message>   ("set_on_message"   );
    lc.def<&lhttp_socket::set_on_acception> ("set_on_acception" );
    lc.def<&lhttp_socket::set_on_connection>("set_on_connection");
    lc.def<&lhttp_socket::set_on_disconnect>("set_on_disconnect");
    lc.def<&lhttp_socket::file_description> ("file_description" );

    lc.def<&lhttp_socket::get_url>   ("get_url");
    lc.def<&lhttp_socket::get_body>  ("get_body" );
    lc.def<&lhttp_socket::get_method>("get_method");
    lc.def<&lhttp_socket::get_status>("get_status");
    lc.def<&lhttp_socket::get_head_field>("get_head_field");

    return 0;
}

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
    lc.def<&lsql::join>  ( "join"  );

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
    lc.def<&lmongo::join>  ( "join"  );

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
    lc.def<&llog::join>  ("join");
    lc.def<&llog::start> ("start");
    lc.def<&llog::write> ("write");
    lc.def<&llog::mkdir_p> ("mkdir_p");

    return 0;
}
