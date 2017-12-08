#include <signal.h>

#include "leventloop.h"
#include "lstate.h"
#include "ltools.h"
#include "lnetwork_mgr.h"

#include "../ev/ev_def.h"
#include "../net/socket.h"

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
}

leventloop::~leventloop()
{
    if ( ansendings ) delete []ansendings;
    ansendings = NULL;
    ansendingcnt =  0;
    ansendingmax =  0;
}

int32 leventloop::exit()
{
    ev_loop::quit();
    return 0;
}

int32 leventloop::backend()
{
    assert( "backend uninit",backend_fd >= 0 );
    assert( "lua state NULL",L );

    loop_done = false;
    lua_gc(L, LUA_GCSTOP, 0); /* 用自己的策略控制gc */

    return run(); /* this won't return until backend stop */
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
    lua_pushcfunction(L,traceback);

    int signum = 0;
    int32 top = lua_gettop(L);
    while (sig_mask != 0)
    {
        if (sig_mask & 1)
        {
            lua_getglobal( L,"sig_handler" );
            lua_pushinteger( L,signum );
            if ( expect_false( LUA_OK != lua_pcall(L,1,0,top) ) )
            {
                ERROR( "signal call lua fail:%s",lua_tostring(L,-1) );
                lua_pop( L,1 ); /* pop error message */
            }
        }
        ++signum;
        sig_mask = (sig_mask >> 1);
    }

    sig_mask = 0;
    lua_remove(L,top); /* remove traceback */
}

int32 leventloop::pending_send( class socket *s  )
{
    // 0位是空的，不使用
    ++ansendingcnt;
    array_resize( ANSENDING,ansendings,ansendingmax,ansendingcnt + 1,EMPTY );
    ansendings[ansendingcnt] = s;

    return ansendingcnt;
}

void leventloop::remove_pending( int32 pending )
{
    assert( "illegal remove pending" ,pending > 0 && pending < ansendingmax );

    ansendings[pending] = NULL;
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

    int32 pos = 0;
    class socket *_socket = NULL;

    for ( int32 i = 1;i <= ansendingcnt;i ++ )/* 0位是空的，不使用 */
    {
        if ( !(_socket = ansendings[i]) ) /* 可能调用了remove_sending */
        {
            continue;
        }

        int32 pending = _socket->get_pending();
        assert( "invoke sending index not match",i == pending );

        /* 处理发送,
         * return: < 0 error,= 0 success,> 0 bytes still need to be send
         */
        if ( _socket->send() <= 0 ) continue;

        /* 还有数据，处理sendings数组移动，防止中间留空 */
        if ( i > pos + 1 )
        {
            ++pos;
            pending = pos;
            ansendings[pos]  = _socket;
        }
        _socket->set_pending( pending );
        /* 数据未发送完，也不需要移动，则do nothing */
    }

    ansendingcnt = pos;
    assert( "invoke sending sending counter fail",
        ansendingcnt >= 0 && ansendingcnt < ansendingmax );
}

void leventloop::running()
{
    invoke_sending ();
    invoke_signal  ();

    lnetwork_mgr::instance()->invoke_delete();

    lua_gc(L, LUA_GCSTEP, 100);
}