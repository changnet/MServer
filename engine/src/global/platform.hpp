#pragma once

// https://blog.kowalczyk.info/article/j/guide-to-predefined-macros-in-c-compilers-gcc-clang-msvc-etc..html
#if defined(__linux__)
    #define __OS_NAME__ "LINUX"
#elif defined(_WIN32) || defined(_WIN64)
    #define __windows__
    #define __OS_NAME__ "WINDOWS"

    // 排除windows.h一些不常用的头文件，一些头文件甚至会引起冲突
    // 例如windows.h默认引用Winsock.h而不是Winsock2.h
    // 如果有功能被排除掉，显式include对应的头文件即可
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif

    // windows.h默认会覆盖std中的std::max和std::min，定义这个宏可禁用它
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif

    // 去掉不使用_s函数的警告
    #define _CRT_SECURE_NO_WARNINGS

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
