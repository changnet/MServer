#pragma once

#include "thread_message.hpp"

struct lua_State;

// 主线程
class MainThread final
{
public:
    MainThread();
    ~MainThread();

private:
    lua_State *L_;
    ThreadMessageQueue message_;
};
