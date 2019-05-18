#include "thread.h"
#include "thread_mgr.h"

thread_mgr::thread_mgr()
{
}

thread_mgr::~thread_mgr()
{
}

/* 添加一个待管理thread */
void thread_mgr::push( class thread *thd )
{
    assert( "thread mgr push NULL",thd );

    _threads[thd->get_id()] = thd;
}

/* 取消管理thread */
void thread_mgr::pop( pthread_t thd_id )
{
    thread_mpt_t::iterator itr = _threads.find( thd_id );
    if ( itr != _threads.end() )
    {
        _threads.erase( itr );
    }
}

/* 停止并join所有线程 */
void thread_mgr::stop()
{
    thread_mpt_t threads( _threads );

    // 循环里会改变_threads
    thread_mpt_t::iterator itr = threads.begin();
    while ( itr != threads.end() )
    {
        class thread *_thread = itr->second;
        _thread->stop();

        itr ++;
    }

    _threads.clear();
}

/* 查找没处理完数据的子线程 */
const char *thread_mgr::who_is_busy(
    size_t &finished,size_t &unfinished,bool skip)
{
    thread_mpt_t::const_iterator itr = _threads.begin();
    while ( itr != _threads.end() )
    {
        class thread *thd = itr->second;

        // 这个线程的数据不需要等待它处理完
        // 比如写日志不会回调到主线程，最后会pthread_join等待写完日志
        if ( skip and !thd->is_wait_busy() )
        {
            itr ++;
            continue;
        }

        if ( thd->busy_job(&finished,&unfinished) > 0 ) return thd->get_name();

        itr ++;
    }

    finished = 0;
    unfinished = 0;
    return NULL;
}