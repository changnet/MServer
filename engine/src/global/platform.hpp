#pragma once

// https://blog.kowalczyk.info/article/j/guide-to-predefined-macros-in-c-compilers-gcc-clang-msvc-etc..html
#if defined(__linux__)
    #define __OS_NAME__ "LINUX"
#elif defined(_WIN32) || defined(_WIN64)
    #define __windows__
    #define __OS_NAME__ "WINDOWS"

    #include <windows.h>
    #define PATH_MAX                MAX_PATH
    #define localtime_r(timer, buf) localtime_s(buf, timer)
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
