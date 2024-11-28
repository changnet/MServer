#pragma once

#include "ltools.hpp"
#include "lcpp.hpp"

// C函数，避免名字被demangle，这样可以直接在gdb中调用
extern "C"
{
    /// 强制中断lua的执行，调试用
    void __dbg_break();
    /// 强制打印lua的堆栈，调试用
    const char *__dbg_traceback();
}

namespace llib
{
// 仅打开cpp库
void open_cpp(lua_State *L);

// 打开环境
void open_env(lua_State *L);

// 打开所库及环境
void open_libs(lua_State *L);

// 创建一个lua虚拟机，并打开所有库
lua_State *new_state();

// 销毁虚拟机
lua_State *delete_state(lua_State *L);
}
