#pragma once

#include "lua_cpplib/llog.hpp"
#include "lua_cpplib/lev.hpp"
#include "thread/thread_mgr.hpp"

/**
 * 控制static或者global变量，的创建、销毁顺序，避免相互依赖，影响内存泄漏计数
 * static initialization order fiasco(https://isocpp.org/wiki/faq/ctors)
 *
 * 不放这里的静态变量只能是局部静态变量。如果是全局变量得在uninitialize函数手动
 * 释放内存
 *
 * initializer只放业务无关的初始化，比如全局锁、ssl初始化...
 * 业务逻辑的初始化放initialize函数
 * 业务逻辑的销毁放uninitialize函数。业务逻辑必须在这里销毁，不能等到static对象析构
 * !!! 因为其他局部变量在main函数之后销毁了
 */

// 统一管理全局变量、静态变量
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
    static class LLog *async_logger()
    {
        return _async_log;
    }
    static class ThreadMgr *thread_mgr()
    {
        return _thread_mgr;
    }
    static Buffer::ChunkPool *buffer_chunk_pool()
    {
        return _buffer_chunk_pool;
    }

public:
    // 这里负责保存一些全局变量，方便StaticGlobal::X这样调用

    inline static lua_State *L = nullptr; // Lua的虚拟机指针
    inline static bool T       = false; // 当前是否处于关服状态(term)

private:
    class initializer // 提供一个等级极高的初始化
    {
    public:
        ~initializer();
        explicit initializer();
    };

private:
    static class LEV *_ev;
    static class LLog *_async_log;
    static class ThreadMgr *_thread_mgr;
    static Buffer::ChunkPool *_buffer_chunk_pool;

    static class initializer _initializer;
};
