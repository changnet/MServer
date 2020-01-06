#pragma once

/* 事件循环 lua wrap */

#include <lua.hpp>

#include "../global/global.h"
#include "../ev/ev.h"
#include "../ev/ev_watcher.h"

class Socket;

/**
 * event loop, 事件主循环
 */
class LEV : public EV
{
public:
    ~LEV();
    explicit LEV();

    /**
     * 关闭socket、定时器等并退出循环
     */
    int32_t exit(lua_State *L);

    /**
     * 获取帧时间戳，秒
     * 如果服务器卡了，这时间和实时时间对不上
     */
    int32_t time(lua_State *L);

    /**
     * 获取帧时间戳，毫秒
     */
    int32_t ms_time(lua_State *L);

    /**
     * 进入后台循环
     */
    int32_t backend(lua_State *L);

    /**
     * 查看繁忙的线程
     * @param skip 是否跳过被设置为不需要等待的线程
     * @return 线程名字 已处理完等待交付主线程的任务 等待处理的任务
     */
    int32_t who_busy(lua_State *L);

    /**
     * 获取实时时间戳，秒
     */
    int32_t real_time(lua_State *L);

    /**
     * 获取实时时间戳，毫秒
     */
    int32_t real_ms_time(lua_State *L);

    /**
     * 设置主循环单次循环临界时间，当单次循环超过此时间时，将会打印繁忙日志
     * @param critical 临界时间，毫秒
     */
    int32_t set_critical_time(lua_State *L);

    /**
     * 注册信号处理
     * @param sig 信号，如SIGTERM
     * @param action 处理方式
     * 0删除之前的注册，1忽略此信号，其他则回调到脚本sig_handler处理
     */
    int32_t signal(lua_State *L);

    /**
     * 设置app回调时间，不断回调到脚本全局application_ev函数
     * @param interval 回调间隔，毫秒
     */
    int32_t set_app_ev(lua_State *L);

    /**
     * 设置脚本gc统计
     * @param stat boolean,是否统计gc
     * @param reset 可选参数，boolean，是否重置之前的统计数据
     */
    int32_t set_gc_stat(lua_State *L);

    int32_t pending_send(class Socket *s);
    void remove_pending(int32_t pending);

private:
    void running(int64_t ms_now);
    void invoke_signal();
    void invoke_sending();
    void invoke_app_ev(int64_t ms_now);
    void after_run(int64_t old_ms_now, int64_t ms_now);

    EvTstamp wait_time();
    static void sig_handler(int32_t signum);

private:
    typedef class Socket *ANSENDING;

    /* 待发送队列 */
    ANSENDING *ansendings;
    int32_t ansendingmax;
    int32_t ansendingcnt;

    int32_t _critical_tm;     // 每次主循环的临界时间，毫秒
    EvTstamp _lua_gc_tm;      // 上一次gc的时间戳
    int64_t _next_app_ev_tm;  // 下次运行脚本主循环的时间戳
    int32_t _app_ev_interval; // 多少毫秒加高一次到脚本

    bool _lua_gc_stat; // 是否统计lua gc时间

    static uint32_t sig_mask;
};
