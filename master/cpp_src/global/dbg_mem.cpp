#include "dbg_mem.h"

#include "assert.h"
#include "../config.h"

#include <pthread.h>

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

#ifdef _MEM_DEBUG_
static pthread_mutex_t *counter_mutex()
{
    static pthread_mutex_t _mutex;
    assert( "global memory counter mutex error",
        0 == pthread_mutex_init( &_mutex,NULL ) );
    return &_mutex;
}

/* Static initialization
 * https://en.cppreference.com/w/cpp/language/initialization
 * this initialization happens before any dynamic initialization.
 * static initialization order fiasco(https://isocpp.org/wiki/faq/ctors)
 * we use this to make sure no other static variable allocate memory before
 * memory counter initialization.
 */
static pthread_mutex_t *_mem_mutex_ = NULL;

class global_static
{
public:
    global_static() 
    {
        assert( "memory counter mutex is NULL",!_mem_mutex_ );
        _mem_mutex_ = counter_mutex();
    }

    ~global_static() {_mem_mutex_ = NULL;}
};

/* Dynamic initialization */
const static global_static gs;

void *operator new(size_t size) EXCEPT
{
    assert( "memory counter mutex is NULL",_mem_mutex_ );

    pthread_mutex_lock( _mem_mutex_ );
    ++g_counter;
    pthread_mutex_unlock( _mem_mutex_ );

    return ::malloc(size);
}

void *operator new(size_t size,const std::nothrow_t& nothrow_value) NOEXCEPT
{
    assert( "memory counter mutex is NULL",_mem_mutex_ );

    pthread_mutex_lock( _mem_mutex_ );
    ++g_counter;
    pthread_mutex_unlock( _mem_mutex_ );

    return ::malloc(size);
}

void operator delete(void* ptr) NOEXCEPT
{
    assert( "memory counter mutex is NULL",_mem_mutex_ );

    pthread_mutex_lock( _mem_mutex_ );
    --g_counter;
    pthread_mutex_unlock( _mem_mutex_ );

    ::free(ptr);
}

void operator delete(void* ptr,const std::nothrow_t& nothrow_value) NOEXCEPT
{
    assert( "memory counter mutex is NULL",_mem_mutex_ );

    pthread_mutex_lock( _mem_mutex_ );
    --g_counter;
    pthread_mutex_unlock( _mem_mutex_ );

    ::free(ptr);
}

void *operator new[](size_t size) EXCEPT
{
    assert( "memory counter mutex is NULL",_mem_mutex_ );

    pthread_mutex_lock( _mem_mutex_ );
    ++g_counters;
    pthread_mutex_unlock( _mem_mutex_ );
    return ::malloc(size);
}

void *operator new[](size_t size,const std::nothrow_t& nothrow_value) NOEXCEPT
{
    assert( "memory counter mutex is NULL",_mem_mutex_ );

    pthread_mutex_lock( _mem_mutex_ );
    ++g_counters;
    pthread_mutex_unlock( _mem_mutex_ );
    return ::malloc(size);
}

void operator delete[](void* ptr) NOEXCEPT
{
    assert( "memory counter mutex is NULL",_mem_mutex_ );

    pthread_mutex_lock( _mem_mutex_ );
    --g_counters;
    pthread_mutex_unlock( _mem_mutex_ );
    ::free(ptr);
}

void operator delete[](void* ptr,const std::nothrow_t& nothrow_value) NOEXCEPT
{
    assert( "memory counter mutex is NULL",_mem_mutex_ );

    pthread_mutex_lock( _mem_mutex_ );
    --g_counters;
    pthread_mutex_unlock( _mem_mutex_ );
    ::free(ptr);
}

#endif /* _MEM_DEBUG_ */
