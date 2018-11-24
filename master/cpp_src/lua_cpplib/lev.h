#ifndef __LEV_H__
#define __LEV_H__

/* 事件循环 lua wrap */

#include <lua.hpp>

#include "../global/global.h"
#include "../ev/ev.h"
#include "../ev/ev_watcher.h"

class socket;
class lev : public ev
{
public:
    static lev *instance();
    static void uninstance();

    explicit lev( lua_State *L );
    ~lev();

    int32 exit( lua_State *L );
    int32 time( lua_State *L ); // 帧时间
    int32 backend( lua_State *L );
    int32 real_time( lua_State *L ); // 实时时间

    int32 signal( lua_State *L );
    int32 set_app_ev( lua_State *L ); // 设置脚本主循环回调

    int32 pending_send( class socket *s );
    void remove_pending( int32 pending );
private:
    explicit lev( bool singleton );

    void running( int64 ms_now );
    void invoke_signal ();
    void invoke_sending();
    void invoke_app_ev (int64 ms_now);

    ev_tstamp wait_time();
    static void sig_handler( int32 signum );
private:
    typedef class socket *ANSENDING;

    /* 待发送队列 */
    ANSENDING *ansendings;
    int32 ansendingmax;
    int32 ansendingcnt;

    ev_tstamp _lua_gc_tm; // 上一次gc的时间戳
    int64 _next_app_ev_tm; // 下次运行脚本主循环的时间戳
    int32 _app_ev_interval; // 多少毫秒加高一次到脚本

    static uint32 sig_mask;
    static class lev *_loop;
};

#endif /* __LEV_H__ */
