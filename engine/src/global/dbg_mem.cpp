#include "../config.hpp"

#include "dbg_mem.hpp"

#include <new> // std::nothrow_t
#include <atomic>
#include <cstdlib> // malloc free

static std::atomic<int64_t> g_counter(0);
static std::atomic<int64_t> g_counters(0);
void global_mem_counter(int64_t &counter, int64_t &counters)
{
    counter  = g_counter;
    counters = g_counters;
}

#ifndef NMEM_DEBUG

void *operator new(size_t size)
{
    ++g_counter;

    return ::malloc(size);
}

void *operator new(size_t size, const std::nothrow_t &nothrow_value) noexcept
{
    ++g_counter;

    return ::malloc(size);
}

void operator delete(void *ptr) noexcept
{
    --g_counter;

    ::free(ptr);
}

void operator delete(void *ptr, const std::nothrow_t &nothrow_value) noexcept
{
    --g_counter;

    ::free(ptr);
}

void *operator new[](size_t size)
{
    ++g_counters;

    return ::malloc(size);
}

void *operator new[](size_t size, const std::nothrow_t &nothrow_value) noexcept
{
    ++g_counters;
    return ::malloc(size);
}

void operator delete[](void *ptr) noexcept
{
    --g_counters;
    ::free(ptr);
}

void operator delete[](void *ptr, const std::nothrow_t &nothrow_value) noexcept
{
    --g_counters;
    ::free(ptr);
}

////////////////////////////////////////////////////////////////////////////////
// 带size的内存释放函数（Sized Deallocation）
// since C++14 https://isocpp.org/files/papers/n3778.html
// 一般情况下，malloc分配的内存，在libc那里会保存有大小，因此释放的时候不需要传大小
// 但是一些第三方内存分配器，会把大小通过额外的方式保存，释放内存时频繁查询大小
// 因此该函数可以省去查询，提升效率。这里重写后，这个就失效了
// 当然，如果使用了第三方内存分配器，一般也是以重写operator
// new/delete的方式来实现 那这时候也不应该开启这里的内存统计

void operator delete(void *ptr, std::size_t size) noexcept
{
    --g_counter;
    ::free(ptr);
}

void operator delete(void *ptr, std::size_t size, const std::nothrow_t &) noexcept
{
    --g_counter;
    ::free(ptr);
}

void operator delete[](void *ptr, std::size_t size) noexcept
{
    --g_counters;
    ::free(ptr);
}

void operator delete[](void *ptr, std::size_t size, const std::nothrow_t &) noexcept
{
    --g_counters;
    ::free(ptr);
}
////////////////////////////////////////////////////////////////////////////////

#endif /* NMEM_DEBUG */

////////////////////// END OF COUNTER /////////////////////////////////////////

#ifdef NDBG_MEM_TRACE

#endif /* NDBG_MEM_TRACE */
