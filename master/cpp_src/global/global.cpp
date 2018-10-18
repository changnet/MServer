#include "global.h"

#include <execinfo.h>    /* for backtrace */
#include <signal.h>
#include <pthread.h>

#if __cplusplus < 201103L    /* < C++11 */
    #define NOEXCEPT throw()
    #define EXCEPT throw(std::bad_alloc)
#else                       /* if support C++ 2011 */
    #define NOEXCEPT noexcept
    #define EXCEPT
#endif

uint32 g_counter  = 0;
uint32 g_counters = 0;

#ifdef _MEM_DEBUG_
pthread_mutex_t &counter_mutex()
{
    static pthread_mutex_t _mutex;
    assert( "global memory counter mutex error",
        0 == pthread_mutex_init( &_mutex,NULL ) );
    return _mutex;
}

pthread_mutex_t &_mem_mutex_ = counter_mutex();

void *operator new(size_t size) EXCEPT
{
    pthread_mutex_lock( &_mem_mutex_ );
    ++g_counter;
    pthread_mutex_unlock( &_mem_mutex_ );

    return ::malloc(size);
}

void operator delete(void* ptr) NOEXCEPT
{
    pthread_mutex_lock( &_mem_mutex_ );
    --g_counter;
    pthread_mutex_unlock( &_mem_mutex_ );

    ::free(ptr);
}

void *operator new[](size_t size) EXCEPT
{
    ++g_counters;
    return ::malloc(size);
}

void operator delete[](void* ptr) NOEXCEPT
{
    --g_counters;
    ::free(ptr);
}
#endif /* _MEM_DEBUG_ */

/* -rdynamic need while linking. to a file,try backtrace_symbols_fd */
void back_trace(void)
{
    void *array[50] = {0};
    size_t size;
    char **strings;

    size =backtrace(array,50);
    strings = backtrace_symbols(array,size);

    for(size_t i = 0;i < size;i++)
    {
        printf("%s\n",strings[i]);
    }
    free(strings);
}

/* https://isocpp.org/files/papers/N3690.pdf
 * 3.6.3 Termination
 * static 变量的销毁与 static变更创建和std::atexit的调用顺序相关，这里可能并没统计到
 */
void onexit()
{
    PRINTF( "new counter:%d    ----   new[] counter:%d",g_counter,g_counters );
    //back_trace();
}

void new_fail()
{
    ERROR( "no more memory!!!" );
    ::abort();
}

/* test.cpp:40: int main(): log assertion `("wrong",0)' failed. */
void __log_assert_fail (const char *__assertion,
     const char *__file, unsigned int __line, const char *__function)
{
    ERROR( "%s:%d:%s:log assertion '%s' failed",
            __file,__line,__function,__assertion );
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
void signal_block()
{
    //--屏蔽所有信号
    sigset_t mask;
    sigemptyset( &mask );
    sigaddset( &mask, SIGINT  );
    sigaddset( &mask, SIGTERM );

    pthread_sigmask( SIG_BLOCK, &mask, NULL );
}
