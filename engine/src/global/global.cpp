#include "global.hpp"

#ifdef __windows__
    #include <DbgHelp.h>
LONG __unhandled_exception_filte(_EXCEPTION_POINTERS *exception)
{
    // http://www.debuginfo.com/articles/effminidumps.html#introduction
    // http://www.debuginfo.com/examples/effmdmpexamples.html
    HANDLE hFile = CreateFile("master.dmp", GENERIC_READ | GENERIC_WRITE, 0,
                              NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if ((hFile != nullptr) && (hFile != INVALID_HANDLE_VALUE))
    {
        // Create the minidump
        MINIDUMP_EXCEPTION_INFORMATION mdei;

        mdei.ThreadId          = GetCurrentThreadId();
        mdei.ExceptionPointers = exception;
        mdei.ClientPointers    = FALSE;

        MINIDUMP_TYPE mdt =
            (MINIDUMP_TYPE)(MiniDumpWithFullMemory | MiniDumpWithFullMemoryInfo
                            | MiniDumpWithHandleData | MiniDumpWithThreadInfo
                            | MiniDumpWithUnloadedModules);

        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                          mdt, (exception != 0) ? &mdei : 0, 0, 0);

        CloseHandle(hFile);
    }

    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

#ifdef __linux__
    #include <execinfo.h> /* for backtrace */
#endif

/* -rdynamic need while linking. to a file,try backtrace_symbols_fd */
void back_trace(void)
{
#ifdef __linux__
    void *array[50] = {0};
    size_t size;
    char **strings;

    size    = backtrace(array, 50);
    strings = backtrace_symbols(array, size);

    for (size_t i = 0; i < size; i++)
    {
        printf("%s\n", strings[i]);
    }
    free(strings);
#endif
}

/**
 * 格式化std::string，需要C++20才有std::format，这个暂时用一下
 */
std::string std_format(const char *fmt, ...)
{
    char buff[1024];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buff, sizeof(buff), fmt, args);
    va_end(args);

    return std::string(buff); // std::move
}
