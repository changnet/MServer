#include <signal.h>

#include "lev.h"
#include "ltools.h"

#include "../net/socket.h"

#include "../system/static_global.h"

uint32 lev::sig_mask = 0;

lev::lev()
{
    ansendings = NULL;
    ansendingmax =  0;
    ansendingcnt =  0;

    _lua_gc_tm = 0;
    _critical_tm = -1;
    _app_ev_interval = 0;

    _lua_gc_stat = false;
}

lev::~lev()
{
    if ( ansendings ) delete []ansendings;
    ansendings = NULL;
    ansendingcnt =  0;
    ansendingmax =  0;
}

int32 lev::exit( lua_State *L )
{
    ev::quit();
    return 0;
}

int32 lev::backend( lua_State *L )
{
    assert( "backend uninit",backend_fd >= 0 );

    loop_done = false;
    lua_gc(L, LUA_GCSTOP, 0); /* 停止主动gc,用自己的策略控制gc */

    return run(); /* this won't return until backend stop */
}

// 帧时间
int32 lev::time( lua_State *L )
{
    lua_pushinteger( L,ev_rt_now );
    return 1;
}

int32 lev::ms_time( lua_State *L ) // 帧时间，ms
{
    lua_pushinteger( L,ev_now_ms );
    return 1;
}

int32 lev::who_busy( lua_State *L ) // 看下哪条线程繁忙
{
    bool skip = lua_toboolean( L,1 );

    size_t finished = 0;
    size_t unfinished = 0;
    const char *who =
        static_global::thread_mgr()->who_is_busy(finished,unfinished,skip);

    if ( !who ) return 0;

    lua_pushstring ( L,who );
    lua_pushinteger( L,finished );
    lua_pushinteger( L,unfinished );

    return 3;
}

// 实时时间
int32 lev::real_time( lua_State *L )
{
    lua_pushinteger( L,get_time() );
    return 1;
}

// 实时时间
int32 lev::real_ms_time( lua_State *L )
{
    lua_pushinteger( L,get_ms_time() );
    return 1;
}

// 设置lua gc参数
int32 lev::set_gc_stat( lua_State *L )
{
    _lua_gc_stat = lua_toboolean( L,1 );

    if (lua_isboolean( L,2 ) && lua_toboolean( L,2 ))
    {
        static_global::statistic()->reset_lua_gc();
    }

    return 0;
}

int32 lev::signal( lua_State *L )
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

int32 lev::set_app_ev( lua_State *L ) // 设置脚本主循环回调
{
    // 主循环不要设置太长的循环时间，如果太长用定时器就好了
    int32 interval = luaL_checkinteger(L, 1);
    if ( interval < 0 || interval > 1000 )
    {
        return luaL_error( L,"illegal argument" );
    }

    _app_ev_interval = interval;
    return 0;
}

int32 lev::set_critical_time( lua_State *L ) // 设置主循环临界时间
{
    _critical_tm = luaL_checkinteger(L, 1);

    return 0;
}

void lev::sig_handler( int32 signum )
{
    sig_mask |= ( 1 << signum );
}

void lev::invoke_signal()
{
    static lua_State *L = static_global::state();
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

int32 lev::pending_send( class socket *s  )
{
    // 0位是空的，不使用
    ++ansendingcnt;
    array_resize( ANSENDING,ansendings,
        ansendingmax,ansendingcnt + 1,array_noinit );
    ansendings[ansendingcnt] = s;

    return ansendingcnt;
}

void lev::remove_pending( int32 pending )
{
    assert( "illegal remove pending" ,pending > 0 && pending < ansendingmax );

    ansendings[pending] = NULL;
}

/* 把数据攒到一起，一次发送
 * 好处是：把包整合，减少发送次数，提高效率
 * 坏处是：需要多一个数组管理；如果发送的数据量很大，在逻辑处理过程中就不能利用带宽
 * 然而，游戏中包多，但数据量不大
 */
void lev::invoke_sending()
{
    if ( ansendingcnt <= 0 ) return;

    int32 pos = 0;
    class socket *skt = NULL;

    /* 0位是空的，不使用 */
    for ( int32 pending = 1;pending <= ansendingcnt;pending ++ )
    {
        if ( !(skt = ansendings[pending]) ) /* 可能调用了remove_sending */
        {
            continue;
        }

        assert( "pending index not match",pending == skt->get_pending() );

        /* 处理发送,
         * return: < 0 error,= 0 success,> 0 bytes still need to be send
         */
        if ( skt->send() <= 0 ) continue;

        /* 还有数据，处理sendings数组移动，防止中间留空 */
        if ( pending > pos )
        {
            ++pos;
            ansendings[pos]  = skt;
            skt->set_pending( pos );
        }
        assert( "new pending index not match",pos == skt->get_pending());
        /* 数据未发送完，也不需要移动，则do nothing */
    }

    ansendingcnt = pos;
    assert( "invoke sending sending counter fail",
        ansendingcnt >= 0 && ansendingcnt < ansendingmax );
}

void lev::invoke_app_ev (int64 ms_now)
{
    static lua_State *L = static_global::state();

    if (!_app_ev_interval || _next_app_ev_tm > ms_now ) return;

    // TODO:以当前时间为起点。服务器卡了，回调次数就少了，以后有需要再改
    _next_app_ev_tm = ms_now + _app_ev_interval;

    lua_pushcfunction(L,traceback);
    lua_getglobal( L,"application_ev" );
    lua_pushinteger( L,ms_now );
    if ( expect_false( LUA_OK != lua_pcall(L,1,0,1) ) )
    {
        ERROR( "invoke_app_ev fail:%s",lua_tostring(L,-1) );
        lua_pop( L,1 ); /* pop error message */
    }

    lua_pop( L,1 ); /* remove traceback */
}

// 计算距离下一次循环时的时间(毫秒)
ev_tstamp lev::wait_time()
{
    // TODO:如果有数据未发送，尽快发送(暂定10毫秒，后面再做调试)
    if (ansendingcnt > 0) return 10;

    ev_tstamp waittime = ev::wait_time();

    // 在定时器和下一次脚本回调之间选一个最接近的数据
    if (_app_ev_interval)
    {
        waittime = MATH_MIN( (_next_app_ev_tm - ev_now_ms),waittime );
    }

    if (expect_false (waittime < EPOLL_MIN_TM))
    {
        waittime = EPOLL_MIN_TM;
    }

    return waittime;
}

void lev::running( int64 ms_now )
{
    invoke_sending ();
    invoke_signal  ();
    invoke_app_ev  (ms_now);

    static_global::network_mgr()->invoke_delete();

    static lua_State *L = static_global::state();

    // TODO:每秒gc一个步骤，太频繁浪费性能,间隔太大导致内存累积，需要根据项目调整
    if (_lua_gc_tm != ev_rt_now)
    {
        _lua_gc_tm = ev_rt_now;

        if ( !_lua_gc_stat )
        {
            lua_gc(L, LUA_GCSTEP, 100);
        }
        else
        {
            STAT_TIME_BEG();
            lua_gc(L, LUA_GCSTEP, 100);
            static_global::statistic()->add_lua_gc( STAT_TIME_END() );
        }
    }
}

void lev::after_run(int64 ms_old,int64 ms_now )
{
    if ( expect_false(_critical_tm < 0) ) return;

    int64 ms = ms_now - ms_old;
    if ( ms > _critical_tm )
    {
        PRINTF("ev busy:%d msec",ms);
    }
}
