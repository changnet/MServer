#ifndef __STATIC_GLOBAL_H__
#define __STATIC_GLOBAL_H__

#include "statistic.h"
#include "../log/thread_log.h"
#include "../lua_cpplib/lev.h"
#include "../net/io/ssl_mgr.h"
#include "../lua_cpplib/lstate.h"
#include "../net/codec/codec_mgr.h"
#include "../lua_cpplib/lnetwork_mgr.h"

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
    static class ssl_mgr *ssl_mgr() { return &_ssl_mgr; }
    static class codec_mgr *codec_mgr() { return &_codec_mgr; }
    static class statistic *statistic() { return &_statistic; }
    static class thread_log *global_log() { return &_global_log; }
    static class lnetwork_mgr *network_mgr() { return &_network_mgr; }
private:
    class initializer
    {
    public:
        ~initializer();
        explicit initializer();
    };
private:
    /* 这个顺序是要求的
     * 同一个unit中的static对象，先声明的先初始化。销毁的时候则反过来
     */
    static class initializer _initializer;
    static class statistic _statistic;
    static class lev _ev;
    static class lstate _state;
    static class thread_log _global_log;
    static class codec_mgr _codec_mgr;
    static class ssl_mgr _ssl_mgr;
    static class lnetwork_mgr _network_mgr;
};

#endif /* __STATIC_GLOBAL_H__ */
