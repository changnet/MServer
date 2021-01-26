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

#endif /* NMEM_DEBUG */

////////////////////// END OF COUNTER /////////////////////////////////////////

#ifdef NDBG_MEM_TRACE

#endif /* NDBG_MEM_TRACE */
