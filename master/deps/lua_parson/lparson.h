#ifndef __LPARSON_H__
#define __LPARSON_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

extern int luaopen_lua_parson( lua_State *L );

#ifdef __cplusplus
}
#endif

#endif /* __LPARSON_H__*/
