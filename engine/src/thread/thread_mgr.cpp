#include "thread_mgr.hpp"

ThreadMgr::ThreadMgr() {}

ThreadMgr::~ThreadMgr() {}

void ThreadMgr::push(class Thread *thd)
{
    assert(thd);

    _threads.push_back(thd);
}

void ThreadMgr::pop(int32_t thd_id)
{
    for (auto iter = _threads.begin(); iter != _threads.end(); iter++)
    {
        if ((*iter)->get_id() == thd_id)
        {
            _threads.erase(iter);
            return;
        }
    }
}

void ThreadMgr::stop()
{
    // 循环里会改变_threads，需要复制一份
    std::vector<Thread *> threads(_threads);

    for (auto thread : threads)
    {
        thread->stop();
    }

    _threads.clear();
}

void ThreadMgr::main_routine()
{
    for (auto thread : _threads)
    {
        int32_t ev = thread->main_event_once();
        if (ev) thread->main_routine(ev);
    }
}

const char *ThreadMgr::who_is_busy(size_t &finished, size_t &unfinished, bool skip)
{
    for (auto thread : _threads)
    {
        // 这个线程的数据不需要等待它处理完
        // 比如写日志不会回调到主线程，最后会pthread_join等待写完日志
        if (skip && !thread->is_wait_busy())
        {
            continue;
        }

        if (thread->busy_job(&finished, &unfinished) > 0)
        {
            return thread->get_name();
        }
    }

    finished   = 0;
    unfinished = 0;
    return nullptr;
}
