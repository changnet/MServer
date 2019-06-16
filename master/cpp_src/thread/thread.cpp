#include <signal.h>
#include <arpa/inet.h>  /* htons */
#include <sys/types.h>
#include <sys/socket.h>

#include "thread.h"
#include "../net/socket.h"
#include "../system/static_global.h"

thread::thread(const char *name)
{
    _fd[0] = -1 ;
    _fd[1] = -1 ;

    _id    = 0;
    _run  = false;
    _join = false;
    _busy = false;

    _name = name;
    _wait_busy = true;

    int32 rv = pthread_mutex_init( &_mutex,NULL );
    if ( 0 != rv )
    {
        FATAL( "pthread_mutex_init error:%s",strerror(errno) );
        return;
    }
}

thread::~thread()
{
    assert( "thread still running",!_run );
    assert( "io watcher not close",!_watcher.is_active() );
    assert( "socket pair not close", -1 == _fd[0] && -1 == _fd[1] );

    if ( !_join ) pthread_detach( pthread_self() );

    int32 rv = pthread_mutex_destroy( &_mutex );
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
void thread::signal_block()
{
    //--屏蔽所有信号
    sigset_t mask;
    sigemptyset( &mask );
    sigaddset( &mask, SIGINT  );
    sigaddset( &mask, SIGTERM );

    pthread_sigmask( SIG_BLOCK, &mask, NULL );
}

/* 开始线程 */
bool thread::start( int32 sec,int32 usec )
{
    /* fd[0]  父进程
     * fd[1]  子线程
     */
    if ( socketpair( AF_UNIX, SOCK_STREAM,IPPROTO_IP,_fd ) < 0 )
    {
        ERROR( "thread socketpair fail:%s",strerror(errno) );
        return false;
    }
    socket::non_block( _fd[0] );    /* 主线程fd需要为非阻塞并加入到主循环监听 */

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
    if ( pthread_create( &_id,NULL,thread::start_routine,(void *)this ) )
    {
        _run = false;
        ::close( _fd[0] );
        ::close( _fd[1] );

        ERROR( "thread start,create fail:%s",strerror(errno) );
        return false;
    }

    _watcher.set( static_global::ev() );
    _watcher.set<thread,&thread::io_cb>( this );
    _watcher.start( _fd[0],EV_READ );

    static_global::thread_mgr()->push( this );

    return true;
}

/* 终止线程 */
void thread::stop()
{
    if ( !_run )
    {
        ERROR( "thread::stop:thread not running" );
        return;
    }

    assert( "thread join zero thread id", 0 != _id );

    _run = false;
    notify_child( NTF_EXIT );
    int32 ecode = pthread_join( _id,NULL );
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

    static_global::thread_mgr()->pop( _id );
}

void thread::do_routine()
{
    while ( true )
    {
        int8 notify = 0;
        int32 sz = ::read( _fd[1],&notify,sizeof(int8) ); /* 阻塞 */
        if ( sz < 0 )
        {
            /* errno variable is thread safe */
            if ( errno == EAGAIN || errno == EWOULDBLOCK )
            {
                this->routine( NTF_NONE );
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
        this->routine( static_cast<notify_t>(notify) );
        _busy = false;

        if ( NTF_EXIT == static_cast<notify_t>(notify) )
        {
            break;
        }
    }
}

/* 线程入口函数 */
void *thread::start_routine( void *arg )
{
    class thread *_thread = static_cast<class thread *>( arg );
    assert( "thread start routine got NULL argument",_thread );

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

void thread::notify_child( notify_t notify )
{
    assert( "notify_child:socket pair not open",_fd[1] >= 0 );

    int8 val = static_cast<int8>(notify);
    int32 sz = ::write( _fd[0],&val,sizeof(int8) );
    if ( sz != sizeof(int8) )
    {
        ERROR( "notify child error:%s",strerror(errno) );
    }
}

void thread::notify_parent( notify_t notify )
{
    assert( "notify_parent:socket pair not open",_fd[0] >= 0 );

    int8 val = static_cast<int8>(notify);
    int32 sz = ::write( _fd[1],&val,sizeof(int8) );
    if ( sz != sizeof(int8) )
    {
        ERROR_R( "notify child error:%s",strerror(errno) );
    }
}

void thread::io_cb( ev_io &w,int32 revents )
{
    int8 event = 0;
    int32 sz = ::read( _fd[0],&event,sizeof(int8) );
    if ( sz < 0 )
    {
        if ( errno == EAGAIN || errno == EWOULDBLOCK )
        {
            assert( "non-block socket,should not happen",false );
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
    else if ( sizeof(int8) != sz )
    {
        FATAL( "socketpair package incomplete,should not happen" );

        return;
    }

    this->notification( static_cast<notify_t>(event) );
}
