#include "global.h"

#include <execinfo.h>    /* for backtrace */
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
void global_mem_counter(uint32 &counter,uint32 &counters)
{
    counter = g_counter;
    counters = g_counters;
}

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

/* test.cpp:40: int main(): log assertion `("wrong",0)' failed. */
void __log_assert_fail (const char *__assertion,
     const char *__file, unsigned int __line, const char *__function)
{
    ERROR( "%s:%d:%s:log assertion '%s' failed",
            __file,__line,__function,__assertion );
}

