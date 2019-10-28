#include "../config.h"

#include "dbg_mem.h"

#include "assert.h"

#include <pthread.h>

#undef new

#if __cplusplus < 201103L    /* < C++11 */
    #define NOEXCEPT throw()
    #define EXCEPT throw(std::bad_alloc)
#else                       /* if support C++ 2011 */
    #define NOEXCEPT noexcept
    #define EXCEPT
#endif

int32 g_counter  = 0;
int32 g_counters = 0;
void global_mem_counter(int32 &counter,int32 &counters)
{
    counter = g_counter;
    counters = g_counters;
}

#ifndef NMEM_DEBUG

/* Static initialization
 * https://en.cppreference.com/w/cpp/language/initialization
 * this initialization happens before any dynamic initialization.
 * static initialization order fiasco(https://isocpp.org/wiki/faq/ctors)
 * we use this to make sure no other static variable allocate memory before
 * memory counter initialization.
 */

static inline pthread_mutex_t *mem_mutex()
{
    static pthread_mutex_t _mutex;
    ASSERT(
        0 == pthread_mutex_init( &_mutex,NULL ),
        "global memory counter mutex error" );

    return &_mutex;
}

void *operator new(size_t size) EXCEPT
{
    pthread_mutex_t *mutex = mem_mutex();

    pthread_mutex_lock( mutex );
    ++g_counter;
    pthread_mutex_unlock( mutex );

    return ::malloc(size);
}

void *operator new(size_t size,const std::nothrow_t& nothrow_value) NOEXCEPT
{
    pthread_mutex_t *mutex = mem_mutex();

    pthread_mutex_lock( mutex );
    ++g_counter;
    pthread_mutex_unlock( mutex );

    return ::malloc(size);
}

void operator delete(void* ptr) NOEXCEPT
{
    pthread_mutex_t *mutex = mem_mutex();

    pthread_mutex_lock( mutex );
    --g_counter;
    pthread_mutex_unlock( mutex );

    ::free(ptr);
}

void operator delete(void* ptr,const std::nothrow_t& nothrow_value) NOEXCEPT
{
    pthread_mutex_t *mutex = mem_mutex();

    pthread_mutex_lock( mutex );
    --g_counter;
    pthread_mutex_unlock( mutex );

    ::free(ptr);
}

void *operator new[](size_t size) EXCEPT
{
    pthread_mutex_t *mutex = mem_mutex();

    pthread_mutex_lock( mutex );
    ++g_counters;
    pthread_mutex_unlock( mutex );
    return ::malloc(size);
}

void *operator new[](size_t size,const std::nothrow_t& nothrow_value) NOEXCEPT
{
    pthread_mutex_t *mutex = mem_mutex();

    pthread_mutex_lock( mutex );
    ++g_counters;
    pthread_mutex_unlock( mutex );
    return ::malloc(size);
}

void operator delete[](void* ptr) NOEXCEPT
{
    pthread_mutex_t *mutex = mem_mutex();

    pthread_mutex_lock( mutex );
    --g_counters;
    pthread_mutex_unlock( mutex );
    ::free(ptr);
}

void operator delete[](void* ptr,const std::nothrow_t& nothrow_value) NOEXCEPT
{
    pthread_mutex_t *mutex = mem_mutex();

    pthread_mutex_lock( mutex );
    --g_counters;
    pthread_mutex_unlock( mutex );
    ::free(ptr);
}

#endif /* NMEM_DEBUG */

////////////////////// END OF COUNTER /////////////////////////////////////////

#ifdef NDBG_MEM_TRACE

#endif /* NDBG_MEM_TRACE */
