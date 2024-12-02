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

protected:
    virtual void routine_once(int32_t ev) override;
    virtual bool initialize() override;
    virtual bool uninitialize() override;

private:
    lua_State *L_;
    std::string entry_; // lua脚本入口文件
    ThreadMessageQueue message_;
};
