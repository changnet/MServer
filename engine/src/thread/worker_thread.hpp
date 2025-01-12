#pragma once

#include <array>
#include "ev/timer.hpp"
#include "thread_context.hpp"

struct lua_State;

// Worker线程
class WorkerThread final: public ThreadContext
{
public:
    virtual ~WorkerThread();
    explicit WorkerThread(const std::string &name);

    /**
     * 启动worker线程
     * @param path 入口文件lua脚本
     * @param addr 当前worker的地址
     * @param ... 其他参数
     */
    int32_t start(lua_State *L);
    /**
     * @brief 停止线程
     * @param join 如果从主线程停止，可join此子线程。子线程主动停止不可join
     */
    void stop(bool join);
    // 是否运行中
    bool is_start() const
    {
        return !stop_;
    }
    /**
     * @brief 启动定时器
     * @param id 定时器唯一id
     * @param after N毫秒秒后第一次执行
     * @param repeat 重复执行间隔，毫秒数
     * @param policy 定时器重新规则时的策略
     * @return 成功返回>=1,失败返回值<0
     */
    int32_t timer_start(int32_t id, int64_t after, int64_t repeat,
        int32_t policy)
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
 
protected:

    void routine();
    bool initialize();
    bool uninitialize();

private:
    bool stop_;
    lua_State *L_;
    std::string name_;
    std::thread thread_;
    TimerMgr timer_mgr_;
    std::array<std::string, 6> params_; // worker启动参数（第一个必定是入口文件路径）
};
