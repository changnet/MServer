#pragma once

#include "ltools.hpp"
#include "lcpp.hpp"

extern "C"
{
    /// 强制中断lua的执行，调试用
    void __dbg_break();
    /// 强制打印lua的堆栈，调试用
    const char *__dbg_traceback();
}

// lua状态机
class LState
{
public:
    ~LState();
    explicit LState();

    inline lua_State *state()
    {
        return L;
    }

private:
    void open_cpp();

    lua_State *L;
};

