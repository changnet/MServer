#pragma once

// https://blog.kowalczyk.info/article/j/guide-to-predefined-macros-in-c-compilers-gcc-clang-msvc-etc..html
#if defined(__linux__)
    #define __OS_NAME__ "linux"
#elif defined(_WIN32) || defined(_WIN64)
    #define __windows__
    #define __OS_NAME__ "windows"
    #define WIN32_LEAN_AND_MEAN
#endif

#ifdef __MINGW64__
    #define __MINGW__
    #define __COMPLIER_ "mingw64"
    // __VERSION__已定义
#elif __GNUC__
    // https://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html
    #define __COMPLIER_ "gcc"
    // __VERSION__已定义
#endif
