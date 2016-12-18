#include "ltimer.h"
#include "ltools.h"
#include "leventloop.h"
#include "../ev/ev_def.h"

ltimer::ltimer( lua_State *L )
    :L (L)
{
    ref_self      = 0;
    ref_callback  = 0;
}

ltimer::~ltimer()
{
    LUA_UNREF( ref_self     );
    LUA_UNREF( ref_callback );
}

int32 ltimer::start()
{
    ev_tstamp after  = static_cast<ev_tstamp>( luaL_checknumber( L,1 ) );
    ev_tstamp repeat = static_cast<ev_tstamp>( luaL_optnumber( L,2,0. ) );

    if ( after < 0. or repeat < 0. )
    {
        return luaL_error( L,"negative timer argument" );
    }

    if ( !ref_callback )
    {
        return luaL_error( L,"need to set callback function before timer start" );
    }

    /* 理论上可以多次start而不影响，但会有性能上的消耗 */
    if ( timer.is_active() )
    {
        return luaL_error( L,"timer already start" );
    }

    class ev_loop *loop = static_cast<class ev_loop *>( leventloop::instance() );
    timer.set( loop );
    timer.set<ltimer,&ltimer::callback>( this );
    timer.start( after,repeat );

    return 0;
}

int32 ltimer::stop()
{
    timer.stop();

    return 0;
}

int32 ltimer::active()
{
    lua_pushboolean( L,timer.is_active() );

    return 1;
}

int32 ltimer::set_self()
{
    LUA_REF( ref_self );
    return 0;
}

int32 ltimer::set_callback()
{
    LUA_REF( ref_callback );

    return 0;
}

void ltimer::callback( ev_timer &w,int32 revents )
{
    assert( "libev timer cb error",!(EV_ERROR & revents) );

    lua_pushcfunction(L,traceback);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_callback);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_self);

    if ( expect_false( LUA_OK != lua_pcall(L,1,0,-3) ) )
    {
        ERROR( "timer call back fail:%s\n",lua_tostring(L,-1) );
        lua_pop(L,1); /* remove error message traceback */
    }
    lua_pop(L,1); /* remove traceback */
}
