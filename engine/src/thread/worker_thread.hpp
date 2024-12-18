#pragma once

#include <array>
#include "thread.hpp"
#include "thread_message.hpp"

struct lua_State;

// Worker线程
class WorkerThread final: public Thread
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
    // 是否运行中
    bool is_start() const
    {
        return !stop_;
    }
    /**
     * @brief 往主线程消息队列push一个消息并唤醒主线程
     * @param msg 消息指针
     */
    void push_message(ThreadMessage *msg)
    {
        message_.push(*msg);
        cv_.notify_one(1);
    }
    /**
     * @brief 构造一个message并push到主线程消息队列，同时唤醒主线程
     */
    void emplace_message(int32_t src, int32_t dst, int32_t type, void *udata,
                         int32_t usize)
    {
        message_.emplace(src, dst, type, udata, usize);
        cv_.notify_one(1);
    }

protected:
    virtual void routine_once(int32_t ev) override;
    virtual bool initialize() override;
    virtual bool uninitialize() override;

private:
    lua_State *L_;
    std::array<std::string, 6> params_; // worker启动参数（第一个必定是入口文件路径）
    ThreadMessageQueue message_;
};
