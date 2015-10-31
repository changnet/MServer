#ifndef __LTOOLS_H__
#define __LTOOLS_H__

#define LUA_REF(x)                         \
    if ( x )    LUA_UNREF(x);              \
    x = luaL_ref( L,LUA_REGISTRYINDEX )

#define LUA_UNREF(x)                                                 \
    do                                                               \
    {                                                                \
        if ( x > 0 )    {luaL_unref( L,LUA_REGISTRYINDEX,x );x = 0;} \
    }while( 0 )

#endif /* __LTOOLS_H__ */
