#include "global.h"

#include <execinfo.h> /* for backtrace */

/* -rdynamic need while linking. to a file,try backtrace_symbols_fd */
void back_trace(void)
{
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
}

/* test.cpp:40: int main(): log assertion `("wrong",0)' failed. */
void __log_assert_fail(const char *__assertion, const char *__file,
                       unsigned int __line, const char *__function)
{
    ERROR("%s:%d:%s:log assertion '%s' failed", __file, __line, __function,
          __assertion);
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
