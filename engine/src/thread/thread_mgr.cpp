#include "thread_mgr.hpp"

ThreadMgr::ThreadMgr() {}

ThreadMgr::~ThreadMgr() {}

void ThreadMgr::push(class Thread *thd)
{
    assert(thd);

    threads_.push_back(thd);
}

void ThreadMgr::pop(int32_t thd_id)
{
    for (auto iter = threads_.begin(); iter != threads_.end(); iter++)
    {
        if ((*iter)->get_id() == thd_id)
        {
            threads_.erase(iter);
            return;
        }
    }
}

void ThreadMgr::stop(const Thread *exclude)
{
    // 循环里会改变threads_，需要复制一份
    std::vector<Thread *> threads(threads_);

    for (auto thread : threads)
    {
        if (thread != exclude) thread->stop();
    }

    threads_.clear();
}

void ThreadMgr::main_routine()
{
    for (auto thread : threads_)
    {
        int32_t ev = thread->main_event_once();
        if (ev) thread->main_routine(ev);
    }
}

const char *ThreadMgr::who_is_busy(size_t &finished, size_t &unfinished, bool skip)
{
    for (auto thread : threads_)
    {
        // 这个线程的数据不需要等待它处理完
        // 比如写日志不会回调到主线程，最后会pthread_join等待写完日志
        if (skip && !thread->is_wait_busy())
        {
            continue;
        }

        if (thread->busy_job(&finished, &unfinished) > 0)
        {
            return thread->get_thread_name().c_str();
        }
    }

    finished   = 0;
    unfinished = 0;
    return nullptr;
}
