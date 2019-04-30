#ifndef __STATIC_GLOBAL_H__
#define __STATIC_GLOBAL_H__


#include "../log/thread_log.h"
#include "../lua_cpplib/lev.h"

/* 管理全局static或者global变量
 *
 */
class static_global
{
public:
    static class ev *ev() { return &_ev; }
    static class lev *lua_ev() { return &_ev; }
    static class thread_log *global_log() { return &_global_log; }
private:
    static class lev _ev;
    static class thread_log _global_log;
};

#endif /* __STATIC_GLOBAL_H__ */
