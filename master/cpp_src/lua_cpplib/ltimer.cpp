#include "ltimer.h"
#include "ltools.h"
#include "lstate.h"
#include "../ev/ev_def.h"
#include "../system/static_global.h"

ltimer::ltimer( lua_State *L )
{
    _timer_id = luaL_checkinteger( L,2 );

    _timer.set( static_global::ev() );
    _timer.set<ltimer,&ltimer::callback>( this );
}

ltimer::~ltimer()
{
}

int32 ltimer::set( lua_State *L )
{
    ev_tstamp after  = static_cast<ev_tstamp>( luaL_checknumber( L,1 ) );
    ev_tstamp repeat = static_cast<ev_tstamp>( luaL_optnumber( L,2,0. ) );

    if ( after < 0. or repeat < 0. )
    {
        return luaL_error( L,"negative timer argument" );
    }

    _timer.set( after,repeat );

    return 0;
}

int32 ltimer::start( lua_State *L )
{
    /* 理论上可以多次start而不影响，但会有性能上的消耗 */
    if ( _timer.is_active() )
    {
        return luaL_error( L,"timer already start" );
    }

    if ( _timer.at <= 0. && _timer.repeat <= 0. )
    {
        return luaL_error( L,"timer no repeater interval set" );
    }

    _timer.start();

    return 0;
}

int32 ltimer::stop( lua_State *L )
{
    _timer.stop();

    return 0;
}

int32 ltimer::active( lua_State *L )
{
    lua_pushboolean( L,_timer.is_active() );

    return 1;
}

void ltimer::callback( ev_timer &w,int32 revents )
{
    assert( "libev timer cb error",!(EV_ERROR & revents) );

    static lua_State *L = lstate::instance()->state();

    lua_pushcfunction( L,traceback );
    lua_getglobal( L,"timer_event" );
    lua_pushinteger( L,_timer_id   );

    if ( expect_false( LUA_OK != lua_pcall( L,1,0,1 ) ) )
    {
        ERROR( "timer call back fail:%s\n",lua_tostring( L,-1 ) );
        lua_pop( L,2 ); /* remove error message traceback */
        return;
    }
    lua_pop( L,1 ); /* remove traceback */
}
