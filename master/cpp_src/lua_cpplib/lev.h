#pragma once

/* 事件循环 lua wrap */

#include <lua.hpp>

#include "../global/global.h"
#include "../ev/ev.h"
#include "../ev/ev_watcher.h"

class socket;
class lev : public ev
{
public:
    ~lev();
    explicit lev();

    int32 exit( lua_State *L );
    int32 time( lua_State *L ); // 帧时间
    int32 ms_time( lua_State *L ); // 帧时间，ms
    int32 backend( lua_State *L ); // 进入后台循环
    int32 who_busy( lua_State *L ); // 看下哪条线程繁忙
    int32 real_time( lua_State *L ); // 实时时间
    int32 real_ms_time( lua_State *L ); // 实时时间，毫秒
    int32 set_critical_time( lua_State *L ); // 设置主循环临界时间

    int32 signal( lua_State *L );
    int32 set_app_ev( lua_State *L ); // 设置脚本主循环回调

    int32 set_gc_stat( lua_State *L ); // 设置lua gc参数

    int32 pending_send( class socket *s );
    void remove_pending( int32 pending );
private:
    void running( int64 ms_now );
    void invoke_signal ();
    void invoke_sending();
    void invoke_app_ev (int64 ms_now);
    void after_run(int64 old_ms_now ,int64 ms_now );

    ev_tstamp wait_time();
    static void sig_handler( int32 signum );
private:
    typedef class socket *ANSENDING;

    /* 待发送队列 */
    ANSENDING *ansendings;
    int32 ansendingmax;
    int32 ansendingcnt;

    int32 _critical_tm;   // 每次主循环的临界时间，毫秒
    ev_tstamp _lua_gc_tm; // 上一次gc的时间戳
    int64 _next_app_ev_tm; // 下次运行脚本主循环的时间戳
    int32 _app_ev_interval; // 多少毫秒加高一次到脚本

    bool _lua_gc_stat; // 是否统计lua gc时间

    static uint32 sig_mask;
};
