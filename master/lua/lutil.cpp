#include <lua.hpp>
#include <sys/time.h>

#include "lutil.h"

/* clock_gettime(CLOCK_MONOTONIC)是内核调用，并且不受time jump影响
 * gettimeofday是用户空间调用
 */
static int32 timeofday( lua_State *L )
{
    struct timeval tv;
    if ( gettimeofday( &tv, NULL ) == 0 )
    {
        lua_pushnumber( L, tv.tv_sec );
        lua_pushnumber( L, tv.tv_usec );
        return 2;
    }

    return 0;
}

static const luaL_Reg utillib[] =
{
    {"timeofday", timeofday},
    {NULL, NULL}
};

int32 luaopen_util( lua_State *L )
{
  luaL_newlib(L, utillib);
  return 1;
}
