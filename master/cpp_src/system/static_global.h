#ifndef __STATIC_GLOBAL_H__
#define __STATIC_GLOBAL_H__


#include "../log/thread_log.h"
#include "../lua_cpplib/lev.h"
#include "../lua_cpplib/lstate.h"

/* 管理全局static或者global变量
 * 控制全局、静态变更的创建、销毁顺序，避免影响内存管理
 * static initialization order fiasco(https://isocpp.org/wiki/faq/ctors)
 */
class static_global
{
public:
    static class ev *ev() { return &_ev; }
    static class lev *lua_ev() { return &_ev; }

    static lua_State *state() { return _state.state(); } 

    static class thread_log *global_log() { return &_global_log; }
private:
    class initializer
    {
    public:
        ~initializer();
        explicit initializer();
    };
private:
    static class initializer _initializer;
    static class lev _ev;
    static class lstate _state;
    static class thread_log _global_log;
};

#endif /* __STATIC_GLOBAL_H__ */
