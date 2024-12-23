#pragma once

#include <array>
#include "thread_message.hpp"

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
 
protected:
    void routine();
    void routine_once();
    bool initialize();
    bool uninitialize();

private:
    bool stop_;
    lua_State *L_;
    std::string name_;
    std::thread thread_;
    std::array<std::string, 6> params_; // worker启动参数（第一个必定是入口文件路径）
};
