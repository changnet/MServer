#include "global.h"

#include <execinfo.h>    /* for backtrace */

int g_counter = 0;
int g_counters = 0;

void *operator new(size_t size)
{
    ++g_counter;
    return ::malloc(size);
}

void operator delete(void* ptr)
{
    --g_counter;
    ::free(ptr);
}

void *operator new[](size_t size)
{
    ++g_counters;
    return ::malloc(size);
}

void operator delete[](void* ptr)
{
    --g_counters;
    ::free(ptr);
}

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

void OnExit()
{
    DEBUG( "new counter:%d    ----   new[] counter:%d\n",g_counter,g_counters );
    back_trace();
}
