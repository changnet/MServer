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
