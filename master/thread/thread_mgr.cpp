#include "thread_mgr.h"
#include "thread.h"

thread_mgr *thread_mgr::_instance = NULL;

/* 获取单例 */
thread_mgr *thread_mgr::instance()
{
    if ( !_instance )
        _instance = new thread_mgr();
    
    return _instance;
}

/* 销毁单例 */
void thread_mgr::uninstance()
{
    if ( _instance )
        delete _instance;

    _instance = NULL;
}

thread_mgr::thread_mgr()
{
}

thread_mgr::~thread_mgr()
{
}

/* 添加一个待管理thread */
void thread_mgr::push( class thread *_thread )
{
    assert( "thread mgr push NULL",_thread );
    
    pthread_t pt = _thread->get_id();
    _threads[pt] = _thread;
}

/* 取消管理thread */
class thread *thread_mgr::pop( pthread_t thread_t )
{
    std::map< pthread_t,class thread * >::iterator itr = _threads.find( thread_t );
    if ( itr == _threads.end() )
        return NULL;
    
    class thread *_thread = itr->second;
    _threads.erase( itr );
    
    return _thread;
}

/* 停止并join所有线程 */
void thread_mgr::clear()
{
    std::map< pthread_t,class thread * >::iterator itr = _threads.begin();
    while ( itr != _threads.end() )
    {
        class thread *_thread = itr->second;
        _thread->stop();
        _thread->join();
        
        itr ++;
    }
    
    _threads.clear();
}
