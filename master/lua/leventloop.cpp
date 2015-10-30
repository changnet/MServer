#include <signal.h>

#include "leventloop.h"
#include "lstate.h"
#include "ltools.h"
#include "../net/socket_mgr.h"
#include "../ev/ev_def.h"

uint32 leventloop::sig_mask = 0;
class leventloop *leventloop::_loop = NULL;

class leventloop *leventloop::instance()
{
    if ( !_loop )
    {
        lua_State *L = lstate::instance()->state();
        assert( "NULL lua state",L );
        _loop = new leventloop( L,true );
    }
    
    return _loop;
}

void leventloop::uninstance()
{
    if ( _loop ) delete _loop;
    _loop = NULL;
}

leventloop::leventloop( lua_State *L )
{
    /* leventloop是一个单例，但lclass的机制却要求构造函数是公有的 */
    assert( "leventloop is singleton",false );
}

leventloop::leventloop( lua_State *L,bool singleton )
    : L (L)
{
    assert( "leventloop is singleton",!_loop );
    
    UNUSED( singleton );
    sig_ref = 0;
}

leventloop::~leventloop()
{
    /* 保证此处不再依赖lua_State */
    assert( "lua ref not release",0 == sig_ref );
}

void leventloop::finalize()
{
    LUA_UNREF( sig_ref );
}

int32 leventloop::exit()
{
    ev_loop::quit();
    return 0;
}

int32 leventloop::run ()
{
    assert( "backend uninit",backend_fd >= 0 );
    assert( "lua state NULL",L );

    loop_done = false;
    lua_gc(L, LUA_GCSTOP, 0); /* 用自己的策略控制gc */
    class socket_mgr *s_mgr = socket_mgr::instance();

    while ( !loop_done )
    {
        fd_reify();/* update fd-related kernel structures */
        
        /* calculate blocking time */
        {
            ev_tstamp waittime  = 0.;
            
            
            /* update time to cancel out callback processing overhead */
            time_update ();
            
            waittime = MAX_BLOCKTIME;
            
            if (timercnt) /* 如果有定时器，睡眠时间不超过定时器触发时间，以免睡过头 */
            {
               ev_tstamp to = (timers [HEAP0])->at - mn_now;
               if (waittime > to) waittime = to;
            }
    
            /* at this point, we NEED to wait, so we have to ensure */
            /* to pass a minimum nonzero value to the backend */
            /* 如果还有数据未发送，也只休眠最小时间 */
            if (expect_false (waittime < backend_mintime || s_mgr->pending_size() > 0))
                waittime = backend_mintime;

            backend_poll ( waittime );

            /* update ev_rt_now, do magic */
            time_update ();
        }

        /* queue pending timers and reschedule them */
        timers_reify (); /* relative timers called last */
  
        invoke_pending ();
        s_mgr->invoke_sending ();
        s_mgr->invoke_delete  (); /* after sending */
        invoke_signal  ();
        
        lua_gc(L, LUA_GCSTEP, 100);
    }    /* while */
    
    return 0;
}

int32 leventloop::time()
{
    lua_pushinteger( L,ev_rt_now );
    return 1;
}

int32 leventloop::signal()
{
    int32 sig = luaL_checkinteger(L, 1);
    int32 sig_action = luaL_optinteger( L,2,-1);
    if (sig < 1 || sig > 31 )
    {
        return luaL_error( L,"illegal signal id:%d",sig );
    }

    /* check /usr/include/bits/signum.h for more */
    if ( 0 == sig_action )
        ::signal( sig,SIG_DFL );
    else if ( 1 == sig_action )
        ::signal( sig,SIG_IGN );
    else
        ::signal( sig, sig_handler );

    return 0;
}

void leventloop::sig_handler( int32 signum )
{
    sig_mask |= ( 1 << signum );
}

void leventloop::invoke_signal()
{
    int signum = 0;
    while (sig_mask != 0)
    {
        if (sig_mask & 1)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, sig_ref);
            lua_pushinteger( L,signum );
            if ( expect_false( LUA_OK != lua_pcall(L,1,0,0) ) )
            {
                ERROR( "signal call lua fail:%s\n",lua_tostring(L,-1) );
            }
        }
        ++signum;
        sig_mask = (sig_mask >> 1);
    }

    sig_mask = 0;
}

int32 leventloop::set_signal_ref()
{
    if ( !lua_isfunction( L,1 ) )
    {
        return luaL_error( L,"set_signal_ref,argument illegal,expect function" );
    }
    
    if ( sig_ref )
    {
        LUA_UNREF( sig_ref );
    }

    sig_ref = luaL_ref( L,LUA_REGISTRYINDEX );

    return 0;
}
