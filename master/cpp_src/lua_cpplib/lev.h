#pragma once

/* 事件循环 lua wrap */

#include <lua.hpp>

#include "../global/global.h"
#include "../ev/ev.h"
#include "../ev/ev_watcher.h"

class Socket;
class LEv : public Ev
{
public:
    ~LEv();
    explicit LEv();

    int32_t exit( lua_State *L );
    int32_t time( lua_State *L ); // 帧时间
    int32_t ms_time( lua_State *L ); // 帧时间，ms
    int32_t backend( lua_State *L ); // 进入后台循环
    int32_t who_busy( lua_State *L ); // 看下哪条线程繁忙
    int32_t real_time( lua_State *L ); // 实时时间
    int32_t real_ms_time( lua_State *L ); // 实时时间，毫秒
    int32_t set_critical_time( lua_State *L ); // 设置主循环临界时间

    int32_t signal( lua_State *L );
    int32_t set_app_ev( lua_State *L ); // 设置脚本主循环回调

    int32_t set_gc_stat( lua_State *L ); // 设置lua gc参数

    int32_t pending_send( class Socket *s );
    void remove_pending( int32_t pending );
private:
    void running( int64_t ms_now );
    void invoke_signal ();
    void invoke_sending();
    void invoke_app_ev (int64_t ms_now);
    void after_run(int64_t old_ms_now ,int64_t ms_now );

    EvTstamp wait_time();
    static void sig_handler( int32_t signum );
private:
    typedef class Socket *ANSENDING;

    /* 待发送队列 */
    ANSENDING *ansendings;
    int32_t ansendingmax;
    int32_t ansendingcnt;

    int32_t _critical_tm;   // 每次主循环的临界时间，毫秒
    EvTstamp _lua_gc_tm; // 上一次gc的时间戳
    int64_t _next_app_ev_tm; // 下次运行脚本主循环的时间戳
    int32_t _app_ev_interval; // 多少毫秒加高一次到脚本

    bool _lua_gc_stat; // 是否统计lua gc时间

    static uint32_t sig_mask;
};
