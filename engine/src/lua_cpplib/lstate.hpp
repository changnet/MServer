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

    /**
     * 调用lua全局函数，需要指定返回类型，如call<int>("func", 1, 2, 3)。错误会抛异常
     * @param name 函数名
     * @param Args 参数
     */
    template <typename Ret, typename... Args>
    Ret call(const char *name, Args... args);
    template <typename... Args> void call(const char *name, Args... args);

    inline lua_State *state()
    {
        return L;
    }

private:
    void open_cpp();

    lua_State *L;
};

template <typename... Args> void LState::call(const char *name, Args... args)
{
#ifndef NDEBUG
    lcpp::StackChecker sc(L);
#endif

    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, name);

    // https://stackoverflow.com/questions/69021792/expand-parameter-pack-with-index-using-a-fold-expression
    (lcpp::cpp_to_lua(L, args), ...);

    const size_t nargs = sizeof...(Args);
    if (LUA_OK != lua_pcall(L, (int32_t)nargs, 0, (int32_t)nargs - 2))
    {
        std::string message("call ");
        message = message + name + " :" + lua_tostring(L, -1);
        lua_pop(L, 1); // pop error message

        throw std::runtime_error(message);
    }
    lua_pop(L, 1); // pop traceback function
}

template <typename Ret, typename... Args>
Ret LState::call(const char *name, Args... args)
{
#ifndef NDEBUG
    lcpp::StackChecker sc(L);
#endif

    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, name);

    (lcpp::cpp_to_lua(L, args), ...);

    const size_t nargs = sizeof...(Args);
    if (LUA_OK != lua_pcall(L, (int32_t)nargs, 0, (int32_t)nargs - 2))
    {
        std::string message("call ");
        message = message + name + " :" + lua_tostring(L, -1);
        lua_pop(L, 1); // pop error message

        throw std::runtime_error(message);
    }
    Ret v = lcpp::lua_to_cpp<Ret>(L, -1);
    lua_pop(L, 2); // pop retturn v and traceback function

    return v;
}
