#pragma once

/* 事件循环 lua wrap */

#include <lua.hpp>

#include "ev/ev.hpp"
#include "ev/ev_watcher.hpp"
#include "global/global.hpp"

class Socket;

/**
 * event loop, 事件主循环
 */
class LEV final : public EV
{
public:
    ~LEV();
    explicit LEV();

    /**
     * 启动utc定时器
     * @param id 定时器唯一id
     * @param after N秒后第一次执行
     * @param repeat 重复执行间隔，秒数
     * @param policy 定时器重新规则时的策略
     */
    int32_t periodic_start(lua_State *L);
    /**
     * 停止utc定时器
     * @param id 定时器唯一id
     */
    int32_t periodic_stop(lua_State *L);

    /**
     * 启动定时器
     * @param id 定时器唯一id
     * @param after N毫秒后第一次执行
     * @param repeat 重复执行间隔，毫秒数
     * @param policy 定时器重新规则时的策略
     */
    int32_t timer_start(lua_State *L);
    /**
     * 停止定时器
     * @param id 定时器唯一id
     */
    int32_t timer_stop(int32_t id);

    /**
     * 获取帧时间戳，秒
     * 如果服务器卡了，这时间和实时时间是不一样的
     */
    int64_t time();

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
    int32_t signal(int32_t sig, int32_t action);

    /**
     * 设置app回调时间，不断回调到脚本全局application_ev函数
     * @param interval 回调间隔，毫秒
     */
    void set_app_ev(int32_t interval);

private:
    void running() override;

    void invoke_signal();
    void invoke_app_ev();

    virtual void timer_callback(int32_t id, int32_t revents);

private:
    int32_t _critical_tm; // 每次主循环的临界时间，毫秒

    int32_t _app_repeat; // 脚本主循环回调隔间，毫秒
    int64_t _app_next_tm; // 下次回调脚本主循环的时间，毫秒
};
