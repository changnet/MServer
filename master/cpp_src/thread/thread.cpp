#include <signal.h>
#include <arpa/inet.h>  /* htons */
#include <sys/types.h>
#include <sys/socket.h>

#include "thread.h"
#include "../net/socket.h"
#include "../system/static_global.h"

Thread::Thread(const char *name)
{
    _fd[0] = -1 ;
    _fd[1] = -1 ;

    _id    = 0;
    _run  = false;
    _join = false;
    _busy = false;

    _name = name;
    _wait_busy = true;

    int32_t rv = pthread_mutex_init( &_mutex,NULL );
    if ( 0 != rv )
    {
        FATAL( "pthread_mutex_init error:%s",strerror(errno) );
        return;
    }
}

Thread::~Thread()
{
    ASSERT( !_run, "thread still running" );
    ASSERT( !_watcher.is_active(), "io watcher not close" );
    ASSERT( -1 == _fd[0] && -1 == _fd[1], "socket pair not close" );

    if ( !_join ) pthread_detach( pthread_self() );

    int32_t rv = pthread_mutex_destroy( &_mutex );
    if ( 0 != rv )
    {
        FATAL( "pthread_mutex_destroy error:%s",strerror(errno) );
        return;
    }
}

/* 信号阻塞
 * 多线程中需要处理三种信号：
 * 1)pthread_kill向指定线程发送的信号，只有指定的线程收到并处理
 * 2)SIGSEGV之类由本线程异常的信号，只有本线程能收到。但通常是终止整个进程。
 * 3)SIGINT等通过外部传入的信号(如kill指令)，查找一个不阻塞该信号的线程。如果有多个，
 *   则选择第一个。
 * 因此，对于1，当前框架并未使用。对于2，默认abort并coredump。对于3，我们希望主线程
 * 收到而不希望子线程收到，故在这里要block掉。
 */
void Thread::signal_block()
{
    //--屏蔽所有信号
    sigset_t mask;
    sigemptyset( &mask );
    sigaddset( &mask, SIGINT  );
    sigaddset( &mask, SIGTERM );

    pthread_sigmask( SIG_BLOCK, &mask, NULL );
}

/* 开始线程 */
bool Thread::start( int32_t sec,int32_t usec )
{
    /* fd[0]  父进程
     * fd[1]  子线程
     */
    if ( socketpair( AF_UNIX, SOCK_STREAM,IPPROTO_IP,_fd ) < 0 )
    {
        ERROR( "thread socketpair fail:%s",strerror(errno) );
        return false;
    }
    Socket::non_block( _fd[0] );    /* 主线程fd需要为非阻塞并加入到主循环监听 */

    /* 子线程设置一个超时，阻塞就可以了，没必要用poll(fd太大，不允许使用select) */
    struct timeval tm;
    tm.tv_sec  = sec;
    tm.tv_usec = usec;
    setsockopt(
        _fd[1], SOL_SOCKET, SO_RCVTIMEO, &tm.tv_sec,sizeof(struct timeval) );
    setsockopt(
        _fd[1], SOL_SOCKET, SO_SNDTIMEO, &tm.tv_sec, sizeof(struct timeval) );

    /* 为了防止子线程创建比主线程运行更快，需要先设置标识 */
    _run = true;

    /* 创建线程 */
    if ( pthread_create( &_id,NULL,Thread::start_routine,(void *)this ) )
    {
        _run = false;
        ::close( _fd[0] );
        ::close( _fd[1] );

        ERROR( "thread start,create fail:%s",strerror(errno) );
        return false;
    }

    _watcher.set( StaticGlobal::ev() );
    _watcher.set<Thread,&Thread::io_cb>( this );
    _watcher.start( _fd[0],EV_READ );

    StaticGlobal::thread_mgr()->push( this );

    return true;
}

/* 终止线程 */
void Thread::stop()
{
    if ( !_run )
    {
        ERROR( "thread::stop:thread not running" );
        return;
    }

    ASSERT( 0 != _id, "thread join zero thread id" );

    _run = false;
    notify_child( NT_EXIT );
    int32_t ecode = pthread_join( _id,NULL );
    if ( ecode )
    {
        /* On success, pthread_join() returns 0; on error, it returns an error
         * number.errno is not use.
         * not strerror(errno)
         */
        FATAL( "thread join fail:%s",strerror( ecode ) );
        return;
    }
    _join = true;

    if ( _watcher.is_active() ) _watcher.stop();
    if ( _fd[0] >= 0 ) { ::close( _fd[0] );_fd[0] = -1; }
    if ( _fd[1] >= 0 ) { ::close( _fd[1] );_fd[1] = -1; }

    StaticGlobal::thread_mgr()->pop( _id );
}

void Thread::do_routine()
{
    while ( true )
    {
        int8_t notify = 0;
        int32_t sz = ::read( _fd[1],&notify,sizeof(int8_t) ); /* 阻塞 */
        if ( sz < 0 )
        {
            /* errno variable is thread safe */
            if ( errno == EAGAIN || errno == EWOULDBLOCK )
            {
                this->routine( NT_NONE );
                continue;  // just timeout，超时，需要运行routine，里面有ping机制
            }
            else if ( errno == EINTR )
            {
                continue; // 系统中断，gdb调试的时候经常遇到
            }

            ERROR( "thread socketpair broken,"
                "thread exit,code %d:%s",errno,strerror(errno) );
            // socket error,can't notify( fd[0],ERROR );
            break;
        }
        else if ( 0 == sz )
        {
            ERROR( "thread socketpair close,thread exit" );
            break;
        }

        // TODO:这个变量是辅助用的，出错也没什么。暂不加锁
        _busy = true;
        this->routine( static_cast<NotifyType>(notify) );
        _busy = false;

        if ( NT_EXIT == static_cast<NotifyType>(notify) )
        {
            break;
        }
    }
}

/* 线程入口函数 */
void *Thread::start_routine( void *arg )
{
    class Thread *_thread = static_cast<class Thread *>( arg );
    ASSERT( _thread, "thread start routine got NULL argument" );

    signal_block();  /* 子线程不处理外部信号 */

    if ( !_thread->initialize() )  /* 初始化 */
    {
        ERROR( "thread initialize fail" );
        return NULL;
    }

    _thread->do_routine();

    if ( !_thread->uninitialize() )  /* 清理 */
    {
        ERROR( "thread uninitialize fail" );
        return NULL;
    }

    return NULL;
}

void Thread::notify_child( NotifyType notify )
{
    ASSERT( _fd[1] >= 0, "notify_child:socket pair not open" );

    int8_t val = static_cast<int8_t>(notify);
    int32_t sz = ::write( _fd[0],&val,sizeof(int8_t) );
    if ( sz != sizeof(int8_t) )
    {
        ERROR( "notify child error:%s",strerror(errno) );
    }
}

void Thread::notify_parent( NotifyType notify )
{
    ASSERT( _fd[0] >= 0, "notify_parent:socket pair not open" );

    int8_t val = static_cast<int8_t>(notify);
    int32_t sz = ::write( _fd[1],&val,sizeof(int8_t) );
    if ( sz != sizeof(int8_t) )
    {
        ERROR_R( "notify child error:%s",strerror(errno) );
    }
}

void Thread::io_cb( EVIO &w,int32_t revents )
{
    int8_t event = 0;
    int32_t sz = ::read( _fd[0],&event,sizeof(int8_t) );
    if ( sz < 0 )
    {
        if ( errno == EAGAIN || errno == EWOULDBLOCK )
        {
            ASSERT( false, "non-block socket,should not happen" );
            return;
        }

        FATAL( "thread socket pair broken:%s",strerror(errno) );

        return;
    }
    else if ( 0 == sz )
    {
        if ( _run ) FATAL( "thread socketpair should not close" );

        return;
    }
    else if ( sizeof(int8_t) != sz )
    {
        FATAL( "socketpair package incomplete,should not happen" );

        return;
    }

    this->notification( static_cast<NotifyType>(event) );
}
