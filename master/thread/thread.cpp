#include "thread.h"

thread::thread()
{
    _run  = false;
    _join = false;
    thread_t = 0;
    
    int32 rv = pthread_mutex_init( &mutex,NULL );
    if ( 0 != rv )
    {
        FATAL( "pthread_mutex_init error:%s\n",strerror(errno) );
        return;
    }
}

thread::~thread()
{
    if ( !_join ) pthread_detach( pthread_self() );

    int32 rv = pthread_mutex_destroy( &mutex );
    if ( 0 != rv )
    {
        FATAL( "pthread_mutex_destroy error:%s\n",strerror(errno) );
        return;
    }
}

/* 开始线程 */
bool thread::start()
{
    /* 创建线程 */
    if ( pthread_create( &thread_t,NULL,thread::start_routine,(void *)this ) )
    {
        ERROR( "thread start,create fail:%s\n",strerror(errno) );
        return false;
    }
    
    return true;
}

/* 终止线程 */
void thread::stop()
{
    /* 只读不写，仅在程序结束时由主线程单一调用，暂不加锁 */
    //pthread_mutex_lock( &mutex );
    _run = false;
    //pthread_mutex_unlock( &mutex );
}

/* 线程入口函数 */
void *thread::start_routine( void *arg )
{
    class thread *_thread = static_cast<class thread *>( arg );
    assert( "thread start routine got NULL argument",_thread );
    
    signal_block();  /* 子线程不处理外部信号 */

    _thread->_run = true;
    _thread->routine();
    
    return NULL;
}

/* 获取线程id */
pthread_t thread::get_id()
{
    return thread_t;
}

/* 等待当前线程结束 */
void thread::join()
{
    assert( "thread join zero thread id", 0 != thread_t );

    _join = true;
    int32 rv = pthread_join( thread_t,NULL );
    if ( rv )
    {
        /* On success, pthread_join() returns 0; on error, it returns an error
         * number.errno is not use.
         * not strerror(errno)
         */
        FATAL( "thread join fail:%s\n",strerror(rv) );
        return;
    }
}
