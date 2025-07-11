#pragma once

#include "ev/timer.hpp"
#include "thread/thread_context.hpp"

struct lua_State;

// 主线程
class EV final : public ThreadContext
{
public:
    EV();
    ~EV();

    // 初始化主线程并进入主循环
    void start(int32_t argc, char **argv);
    // 退出主循环
    void stop();

    /**
     * @brief 启动定时器
     * @param id 定时器唯一id
     * @param after N毫秒秒后第一次执行
     * @param repeat 重复执行间隔，毫秒数
     * @param policy 定时器重新规则时的策略
     * @return 成功返回>=1,失败返回值<0
     */
    int32_t timer_start(int32_t id, int64_t after, int64_t repeat, int32_t policy)
    {
        return timer_mgr_.timer_start(id, after, repeat, policy);
    }
    /**
     * @brief 停止定时器并从管理器中删除
     * @param id 定时器唯一id
     * @return 成功返回0
     */
    int32_t timer_stop(int32_t id)
    {
        return timer_mgr_.timer_stop(id);
    }
    /**
     * @brief 启动utc定时器
     * @param id 定时器唯一id
     * @param after N毫秒秒后第一次执行
     * @param repeat 重复执行间隔，毫秒数
     * @param policy 定时器重新规则时的策略
     * @return 成功返回>=1,失败返回值<0
     */
    int32_t periodic_start(int32_t id, int64_t after, int64_t repeat,
                           int32_t policy)
    {
        return timer_mgr_.periodic_start(id, after, repeat, policy);
    }
    /**
     * @brief 停止utc定时器并从管理器中删除
     * @param id 定时器唯一id
     * @return 成功返回0
     */
    int32_t periodic_stop(int32_t id)
    {
        return timer_mgr_.periodic_stop(id);
    }

    /**
     * 创建一个thread message
     * @param size buffer的大小（不包含ThreadMessage本身）
     * @return 消息指针，buffer指针
     */
    int32_t construct_message(lua_State *L);
    /**
     * 销毁一个thread message，非安全操作
     */
    int32_t destruct_message(lua_State *L);

    // 把当前指针push到lua
    int32_t push(lua_State *L, bool gc) override;

private:
    // 进入主循环，除非停服否则不返回
    void routinue();
    void dispatch_message();
    bool init_entry(int32_t argc, char **argv);

private:
    bool stop_; // 退出主循环

    lua_State *L_;
    TimerMgr timer_mgr_;
};
