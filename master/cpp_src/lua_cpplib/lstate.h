#ifndef __LSTATE_H__
#define __LSTATE_H__

struct lua_State;

// lua状态机
class lstate
{
public:
    ~lstate();
    explicit lstate();

    inline lua_State *state() { return L; }
private:
    void open_cpp();

    lua_State *L;
};

#endif /* __LSTATE_H__ */
