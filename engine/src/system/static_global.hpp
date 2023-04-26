#pragma once

#include "../lua_cpplib/llog.hpp"
#include "../lua_cpplib/lev.hpp"
#include "../lua_cpplib/lnetwork_mgr.hpp"
#include "../lua_cpplib/lstate.hpp"
#include "../net/io/ssl_mgr.hpp"
#include "../thread/thread_mgr.hpp"
#include "statistic.hpp"

/**
 * 控制static或者global变量，的创建、销毁顺序，避免相互依赖，影响内存泄漏计数
 * static initialization order fiasco(https://isocpp.org/wiki/faq/ctors)
 *
 * 不放这里的静态变量只能是局部静态变量
 *
 * initializer只放业务无关的初始化，比如全局锁、ssl初始化...
 * 业务逻辑的初始化放initialize函数
 * 业务逻辑的销毁放uninitialize函数。业务逻辑必须在这里销毁，不能等到static对象析构
 * !!! 因为其他局部变量在main函数之后销毁了
 */
class StaticGlobal
{
public:
    static void initialize();   /* 程序运行时初始化 */
    static void uninitialize(); /* 程序结束时反初始化 */

    static class EV *ev()
    {
        return _ev;
    }
    static class LEV *lua_ev()
    {
        return _ev;
    }
    static lua_State *state()
    {
        return _state->state();
    }
    static class SSLMgr *ssl_mgr()
    {
        return _ssl_mgr;
    }
    static class CodecMgr *codec_mgr()
    {
        return _codec_mgr;
    }
    static class LuaBinCodec *lua_bin_codec()
    {
        return _lua_bin_codec;
    }
    static class Statistic *statistic()
    {
        return _statistic;
    }
    static class LLog *async_logger()
    {
        return _async_log;
    }
    static class ThreadMgr *thread_mgr()
    {
        return _thread_mgr;
    }
    static class LNetworkMgr *network_mgr()
    {
        return _network_mgr;
    }
    static Buffer::ChunkPool *buffer_chunk_pool()
    {
        return _buffer_chunk_pool;
    }

private:
    class initializer // 提供一个等级极高的初始化
    {
    public:
        ~initializer();
        explicit initializer();
    };

private:
    static class LEV *_ev;
    static class LState *_state;
    static class SSLMgr *_ssl_mgr;
    static class CodecMgr *_codec_mgr;
    static class LuaBinCodec *_lua_bin_codec;
    static class Statistic *_statistic;
    static class LLog *_async_log;
    static class ThreadMgr *_thread_mgr;
    static class LNetworkMgr *_network_mgr;
    static Buffer::ChunkPool *_buffer_chunk_pool;

    static class initializer _initializer;
};
