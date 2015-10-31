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
    
    ansendings = NULL;
    ansendingmax =  0;
    ansendingcnt =  0;
    sig_ref = 0;
}

leventloop::~leventloop()
{
    /* 保证此处不再依赖lua_State */
    assert( "lua ref not release",0 == sig_ref );
    
    if ( ansendings ) delete []ansendings;
    ansendings = NULL;
    ansendingcnt =  0;
    ansendingmax =  0;
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
            if (expect_false (waittime < backend_mintime || ansendingcnt > 0))
                waittime = backend_mintime;

            backend_poll ( waittime );

            /* update ev_rt_now, do magic */
            time_update ();
        }

        /* queue pending timers and reschedule them */
        timers_reify (); /* relative timers called last */
  
        invoke_pending ();
        invoke_sending ();
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

    LUA_REF( sig_ref );

    return 0;
}

void leventloop::pending_send( class socket *s  )
{
    assert( "double pending send",0 == s->sending );
    // 0位是空的，不使用
    ++ansendingcnt;
    array_resize( ANSENDING,ansendings,ansendingmax,ansendingcnt + 1,EMPTY );
    ansendings[ansendingcnt] = s;
    
    s->sending = ansendingcnt;
}

void leventloop::remove_sending( int32 sending )
{
    assert( "illegal remove sending" ,sending > 0 && sending < ansendingmax );
    
    ansendings[sending] = NULL;
}

/* 把数据攒到一起，一次发送
 * 好处是：把包整合，减少发送次数，提高效率
 * 坏处是：需要多一个数组管理；如果发送的数据量很大，在逻辑处理过程中就不能利用带宽
 * 然而，游戏中包多，但数据量不大
 */
void leventloop::invoke_sending()
{
    if ( ansendingcnt <= 0 )
        return;

    int32 empty = 0;
    int32 empty_max = 0;
    class socket *_socket = NULL;

    for ( int32 i = 1;i <= ansendingcnt;i ++ )/* 0位是空的，不使用 */
    {
        if ( !(_socket = ansendings[i]) ) /* 可能调用了remove_sending */
        {
            ++empty;
            empty_max = i;
            continue;
        }
        
        assert( "invoke sending index not match",i == _socket->sending );

        /* 处理发送 */
        int32 ret = _socket->_send.send( _socket->fd() );

        if ( 0 == ret || (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK) )
        {
            ERROR( "invoke sending unsuccess:%s\n",strerror(errno) );
            _socket->on_disconnect();
            ++empty;
            empty_max = i;
            continue;
        }
        
        /* 处理sendings移动 */
        if ( _socket->_send.data_size() <= 0 )
        {
            _socket->sending = 0;   /* 去除发送标识 */
            _socket->_send.clear(); /* 去除悬空区 */
            ++empty;
            empty_max = i;
        }
        else if ( empty )  /* 将发送数组向前移动，防止中间留空 */
        {
            int32 empty_min = empty_max - empty + 1;
            _socket->sending = empty_min;
            --empty;
        }
        /* 数据未发送完，也不需要移动，则do nothing */
    }

    ansendingcnt -= empty;
    assert( "invoke sending sending counter fail",ansendingcnt >= 0 && ansendingcnt < ansendingmax );
}
