#pragma once

#include "thread.hpp"

struct lua_State;

// Worker线程
class WorkerThread final: public Thread
{
public:
    virtual ~WorkerThread();
    explicit WorkerThread(const std::string &name);

protected:
    virtual void routine(int32_t ev) override;

private:
    lua_State *L;
};
