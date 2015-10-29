#ifndef __LTOOLS_H__
#define __LTOOLS_H__

#define LUA_UNREF(x)                            \
    if ( x > 0 )    luaL_unref( L,LUA_REGISTRYINDEX,x )

#endif /* __LTOOLS_H__ */
