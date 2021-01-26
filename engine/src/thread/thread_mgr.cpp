#include "thread.hpp"
#include "thread_mgr.hpp"

ThreadMgr::ThreadMgr() {}

ThreadMgr::~ThreadMgr() {}

/* 添加一个待管理thread */
void ThreadMgr::push(class Thread *thd)
{
    assert(thd);

    _threads[thd->get_id()] = thd;
}

/* 取消管理thread */
void ThreadMgr::pop(pthread_t thd_id)
{
    ThreadMap::iterator itr = _threads.find(thd_id);
    if (itr != _threads.end())
    {
        _threads.erase(itr);
    }
}

/* 停止并join所有线程 */
void ThreadMgr::stop()
{
    ThreadMap threads(_threads);

    // 循环里会改变_threads
    ThreadMap::iterator itr = threads.begin();
    while (itr != threads.end())
    {
        class Thread *_thread = itr->second;
        _thread->stop();

        itr++;
    }

    _threads.clear();
}

/* 查找没处理完数据的子线程 */
const char *ThreadMgr::who_is_busy(size_t &finished, size_t &unfinished, bool skip)
{
    ThreadMap::const_iterator itr = _threads.begin();
    while (itr != _threads.end())
    {
        class Thread *thd = itr->second;

        // 这个线程的数据不需要等待它处理完
        // 比如写日志不会回调到主线程，最后会pthread_join等待写完日志
        if (skip && !thd->is_wait_busy())
        {
            itr++;
            continue;
        }

        if (thd->busy_job(&finished, &unfinished) > 0) return thd->get_name();

        itr++;
    }

    finished   = 0;
    unfinished = 0;
    return NULL;
}
