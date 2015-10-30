#include "lstate.h"
#include "lclass.h"
#include "lsocket.h"
#include "leventloop.h"
#include "../ev/ev_def.h"

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

int luaopen_ev( lua_State *L );
int luaopen_util( lua_State *L );
int luaopen_socket( lua_State *L );

void lstate::open_cpp()
{
    /* 把当前工作目录加到lua的path */
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    const char *old_path = lua_tostring(L, -1);

    char new_path[PATH_MAX] = {0};
    if ( snprintf( new_path,PATH_MAX,"%s;%s/lua_script/?.lua",old_path,cwd ) >= PATH_MAX )
    {
        ERROR( "lua init,path overflow\n" );
        lua_close( L );
        exit( 1 );
    }

    lua_pop(L, 1);    /* drop old path field */
    lua_pushstring(L, new_path);
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);   /* drop package table */

    luaL_requiref(L, "util", luaopen_util, 1);
    lua_pop(L, 1);  /* remove lib */

    luaopen_ev(L);
    luaopen_socket(L);

    /* when debug,make sure lua stack clean after init */
    assert( "lua stack not clean after init", 0 == lua_gettop(L) );
}

int luaopen_ev( lua_State *L )
{
    lclass<leventloop> lc(L,"Eventloop");
    lc.def<&leventloop::run>   ("run");
    lc.def<&leventloop::time>  ("time");
    lc.def<&leventloop::exit>  ("exit");
    lc.def<&leventloop::signal>("signal");
    lc.def<&leventloop::set_signal_ref>("set_signal_ref");

    lc.set( "EV_READ",EV_READ );
    lc.set( "EV_WRITE",EV_WRITE );
    lc.set( "EV_TIMER",EV_TIMER );
    lc.set( "EV_ERROR",EV_ERROR );
    
    lc.set( "SK_ERROR",socket::SK_ERROR );
    lc.set( "SK_SERVER",socket::SK_SERVER );
    lc.set( "SK_CLIENT",socket::SK_CLIENT );

    return 0;
}

int luaopen_socket( lua_State *L )
{
    lclass<lsocket> lc(L,"Socket");
    lc.def<&lsocket::send>("send");
    lc.def<&lsocket::kill>("kill");
    lc.def<&lsocket::listen> ("listen");
    lc.def<&lsocket::address>("address");
    lc.def<&lsocket::connect>("connect");
    lc.def<&lsocket::raw_send>("raw_send");
    lc.def<&lsocket::set_self>("set_self");
    lc.def<&lsocket::set_read>("set_read");
    lc.def<&lsocket::set_accept>("set_accept");
    lc.def<&lsocket::set_connected>("set_connected");
    lc.def<&lsocket::set_disconnected>("set_disconnected");
    
    lc.set( "ERROR" ,socket::SK_ERROR  );
    lc.set( "SERVER",socket::SK_SERVER );
    lc.set( "CLIENT",socket::SK_CLIENT );
    lc.set( "LISTEN",socket::SK_LISTEN );
    lc.set( "HTTP"  ,socket::SK_HTTP   );

    return 0;
}

static int util_md5( lua_State *L )
{
    lua_pushstring( L,"mde35dfafefsxee4r3" );
    return 1;
}

static const luaL_Reg utillib[] =
{
    {"md5", util_md5},
    {NULL, NULL}
};


int luaopen_util( lua_State *L )
{
  luaL_newlib(L, utillib);
  return 1;
}
