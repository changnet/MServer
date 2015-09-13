#include "lenv.h"
#include "lclass.h"

////////////////////////////////////////////////////////////////////////////////
#include "backend.h"
int luaopen_ev (lua_State *L)
{
    lclass<backend> lc(L,"eventloop");
    lc.def<&backend::run> ("run");
    lc.def<&backend::now> ("now");
    lc.def<&backend::quit>("quit");
    lc.def<&backend::listen>("listen");
    lc.def<&backend::io_kill>("io_kill");
    lc.set( "EV_READ",EV_READ );
    lc.set( "EV_WRITE",EV_WRITE );
    lc.set( "EV_TIMER",EV_TIMER );
    lc.set( "EV_ERROR",EV_ERROR );

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
void lua_initenv( lua_State *L )
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

    lua_pop(L, 1);
    lua_pushstring(L, new_path);
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);

    /* 预加载一些lua脚本以初始化环境 */
    char preload_path[PATH_MAX];
    snprintf( preload_path,PATH_MAX,"lua/%s/%s",spath,LUA_PRELOAD );
    if ( !access(preload_path,F_OK|R_OK) ) /* on success zero is returned */
    {
        if ( luaL_dofile(L,preload_path) )
        {
            const char *err_msg = lua_tostring(L,-1);
            FATAL( "load lua preload file error:%s\n",err_msg );
            return;
        }
        /* remove all elements from stack incase preload file return something */
        lua_settop( L,0 );
    }

    luaL_requiref(L, "util", luaopen_util, 1);
    lua_pop(L, 1);  /* remove lib */

    luaopen_ev(L);

    /* when debug,make sure lua stack clean after init */
    assert( "lua stack not clean after init", 0 == lua_gettop(L) );
}
