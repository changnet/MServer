#pragma once

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
     */
    void start(const char *path);

    
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
     * @brief 用int参数构造一个message并push到主线程消息队列，同时唤醒主线程
     */
    void emplace_message_int(int32_t addr, int32_t type, int32_t v1, int32_t v2)
    {
        message_.emplace(addr, type, v1, v2);
        cv_.notify_one(1);
    }
    /**
     * @brief 用指针参数构造一个message并push到主线程消息队列，同时唤醒主线程
     */
    void emplace_message_ptr(int32_t addr, int32_t type, void *ptr)
    {
        message_.emplace(addr, type, ptr);
        cv_.notify_one(1);
    }

protected:
    virtual void routine_once(int32_t ev) override;
    virtual bool initialize() override;
    virtual bool uninitialize() override;

private:
    lua_State *L_;
    std::string entry_; // lua脚本入口文件
    ThreadMessageQueue message_;
};
