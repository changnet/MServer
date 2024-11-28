#pragma once

#include "thread.hpp"

struct lua_State;

// Worker线程
class WorkerThread final: public Thread
{
public:
    virtual ~WorkerThread();
    explicit WorkerThread(const std::string &name);

private:
    lua_State *L;
};
