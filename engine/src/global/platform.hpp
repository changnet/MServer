#pragma once

#include "../global/types.hpp"

// https://blog.kowalczyk.info/article/j/guide-to-predefined-macros-in-c-compilers-gcc-clang-msvc-etc..html
#if defined(__linux__)
    #define __OS_NAME__ "LINUX"
    #define io_errno    errno
    #define io_strerror strerror
#elif defined(_WIN32) || defined(_WIN64)
    #define __windows__
    #define __OS_NAME__ "WINDOWS"

    #include <windows.h>
    #define PATH_MAX                MAX_PATH
    #define localtime_r(timer, buf) localtime_s(buf, timer)
    #define io_errno                WSAGetLastError()
inline const char *io_strerror(int32_t e)
{
    // https://docs.microsoft.com/zh-cn/windows/win32/api/winbase/nf-winbase-formatmessage?redirectedfrom=MSDN
    thread_local char buff[512] = {0};
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  nullptr, e, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buff,
                  sizeof(buff), nullptr);
    return buff;
}
#endif

// __VERSION__在gcc和mingw中已定义，不用额外定义
#ifdef __MINGW64__
    #define __MINGW__
    #define __COMPLIER_ "mingw64"
#elif __GNUC__
    // https://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html
    #define __COMPLIER_ "gcc"

#elif _MSC_VER
    #define __COMPLIER_ "MSVC"

    #define __VERSION__ XSTR(_MSC_FULL_VER)
#endif
