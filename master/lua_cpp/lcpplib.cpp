#include "lcpplib.h"
#include "lclass.h"

////////////////////////////////////////////////////////////////////////////////
#include "backend.h"
#include "../ev/ev_def.h"
#include "../net/socket.h"
int luaopen_ev (lua_State *L)
{
    lclass<backend> lc(L,"eventloop");
    lc.def<&backend::run> ("run");
    lc.def<&backend::time> ("time");
    lc.def<&backend::exit>("exit");
    lc.def<&backend::listen>("listen");
    lc.def<&backend::connect>("connect");
    lc.def<&backend::raw_send>("raw_send");
    lc.def<&backend::io_kill>("io_kill");
    lc.def<&backend::set_net_ref>("set_net_ref");
    lc.def<&backend::fd_address>("fd_address");
    lc.def<&backend::timer_start>("timer_start");
    lc.def<&backend::timer_kill>("timer_kill");
    lc.def<&backend::set_timer_ref>("set_timer_ref");
    lc.def<&backend::set_signal_ref>("set_signal_ref");
    lc.def<&backend::signal>("signal");

    lc.set( "EV_READ",EV_READ );
    lc.set( "EV_WRITE",EV_WRITE );
    lc.set( "EV_TIMER",EV_TIMER );
    lc.set( "EV_ERROR",EV_ERROR );
    
    lc.set( "SK_ERROR",socket::SK_ERROR );
    lc.set( "SK_SERVER",socket::SK_SERVER );
    lc.set( "SK_CLIENT",socket::SK_CLIENT );


    return 0;
}

////////////////////////////////////////////////////////////////////////////////
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


int luaopen_util (lua_State *L)
{
  luaL_newlib(L, utillib);
  return 1;
}

////////////////////////////////////////////////////////////////////////////////
/* 初始化lua环境 */
void luacpp_init( lua_State *L )
{
    /* 把当前工作目录加到lua的path */
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    const char *old_path = lua_tostring(L, -1);

    char new_path[PATH_MAX] = {0};
    if ( snprintf( new_path,PATH_MAX,"%s;%s/?.lua",old_path,cwd ) >= PATH_MAX )
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

    /* when debug,make sure lua stack clean after init */
    assert( "lua stack not clean after init", 0 == lua_gettop(L) );
}
