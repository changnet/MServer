#include "../config.h"

#include "dbg_mem.h"

#include "assert.h"

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

#endif /* _MEM_DEBUG_ */

////////////////////// END OF COUNTER /////////////////////////////////////////

#ifdef _DBG_MEM_TRACE

#include <utility> // std::forward
#include <cstddef> // ptrdiff_t
#include <unordered_map>

// 记录每次申请的内存块信息
struct chunk
{
    size_t size; // 申请的内存大小
};

// 使用独立的内存分配器，避免调用重写的new函数造成死循环
// http://www.cplusplus.com/reference/memory/allocator/
// 实现所有函数就可以当内存分配器了
template <class T> class allocator
{
public:
    typedef T          value_type;
    typedef size_t     size_type;
    typedef ptrdiff_t  difference_type;
    typedef T*         pointer;
    typedef const T*   const_pointer;
    typedef T&         reference;
    typedef const T&   const_reference;

    // The standard allocator has no data members and is not required to perform
    // any initialization, but the three constructor versions must be defined 
    // (even if they do nothing) to allow for copy-constructions from allocator
    // objects of other types
    allocator() NOEXCEPT {}
    allocator (const allocator& alloc) NOEXCEPT {}
    template <class U> allocator (const allocator<U>& alloc) NOEXCEPT {}

    template <class U> struct rebind
    {
        typedef allocator<U> other;
    };

    pointer address(reference ref) const NOEXCEPT
    {
        return &ref;
    }

    const_pointer address(const_reference ref) const NOEXCEPT
    {
        return &ref;
    }

    pointer allocate (size_type n, const_pointer hint = 0)
    {
        pointer ptr = (pointer) malloc(n);

        return ptr;
    }

    void deallocate (pointer p, size_type n)
    {
        free(p);
    }

    // 返回可申请类型T的最大对象数
    size_type max_size() const NOEXCEPT
    {
        // 2 --> kb --> m --> G
        return 2 * 1024 * 1024 * 1024 / sizeof(T);
    }

    template <class U, class... Args> void construct (U* p, Args&&... args)
    {
        ::new ((void*)p) U (std::forward<Args>(args)...);
    }

    template <class U> void destroy (U* p)
    {
        p->~U();
    }
};

typedef std::unordered_map< int64, struct chunk,std::hash<int64>,std::equal_to<int64>,allocator< std::pair<const int64,chunk> > >   mem_chunk_t;

static inline mem_chunk_t &mem_chunk()
{
    static mem_chunk_t chunks;

    return chunks;
}

// 原来nvwa项目是用一个MAGIC数据来判断是否自己分配的
// 这里改成按指针地址判断，所有分配过的内在都存在一个std::unordered_map中
// 如果不在map中，则不处理。placement new的则不处理

void dbg_mem_tracer::process(void* ptr)
{
    mem_chunk_t &chunks = mem_chunk();
    int64 addr = reinterpret_cast<int64>(ptr);

    // 不是我们重写的new函数分配的内存，则为: placement new
    auto itr = chunks.find(addr);
    if (chunks.end() == itr) return;
}

#endif /* _DBG_MEM_TRACE */
