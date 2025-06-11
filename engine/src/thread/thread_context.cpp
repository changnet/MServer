#include "thread_context.hpp"
#include "system/static_global.hpp"

ThreadContext::ThreadContext()
{
}

ThreadContext::~ThreadContext()
{
    // 析构应该只在主线程执行，没加锁的必要了
    for (auto &x : queue_)
    {
        StaticGlobal::F->destruct(x);
    }
}

void ThreadContext::dispose_message(ThreadMessage *m)
{
    StaticGlobal::F->destruct(m);
}

void ThreadContext::emplace_message(int32_t src, int32_t dst, uint16_t type,
                                    void *udata, int32_t usize)
{
    // 这个写法没法导出到lua
    // template <typename... Args> void emplace_message(Args &&...args)
    // queue_.emplace_back(std::forward<Args>(args)...);

    size_t size      = udata ? usize : 0;
    ThreadMessage *m = StaticGlobal::F->construct<ThreadMessage>(
        size, src, dst, type, usize);
    if (udata) memcpy(m->buffer(), udata, usize);
    {
        std::lock_guard<std::mutex> lg(mutex_);
        queue_.push_back(m);
    }
    cv_.notify_one();
}

void ThreadContextMgr::add_thread_ctx(int32_t addr, ThreadContext* ctx)
{
    StaticGlobal::M->add(addr, ctx);
}

void ThreadContextMgr::del_thread_ctx(int32_t addr)
{
    StaticGlobal::M->del(addr);
}
