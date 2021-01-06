#pragma once

struct lua_State;

// lua状态机
class LState
{
public:
    ~LState();
    explicit LState();

    inline lua_State *state() { return L; }

private:
    void open_cpp();

    lua_State *L;
};
