#pragma once

#include <cassert>

#ifdef NDEBUG
    #define ASSERT(...)
#else
#define __ASSERT1(expr) assert(expr)

// __ASSERT_VOID_CAST is linux implement
#ifdef __linux__
    #define __ASSERT2(expr, why)                                           \
        ((expr) ? __ASSERT_VOID_CAST(0)                                    \
                : __assert_fail(__STRING((why, expr)), __FILE__, __LINE__, \
                                __ASSERT_FUNCTION))
#else
    #define __ASSERT2(expr, why)                                            \
        (void)((!!(expr))                                                   \
               || (_wassert(_CRT_WIDE(#why "," #expr), _CRT_WIDE(__FILE__), \
                            __LINE__),                                      \
                   0))
#endif /* __linux__ */

#define __EXPAND(_1, _2, ASSERT_EXPR, ...) ASSERT_EXPR

// 当只传一个参数时 __EXPAND参数为(expr, __ASSERT2, __ASSERT1)，ASSERT_EXPR对应__ASSERT1
// 当传两个参数时，__EXPAND参数为(expr, msg, __ASSERT2, __ASSERT1)，ASSERT_EXPR对应__ASSERT2
// _UNUSED是因为__EXPAND的参数有一个...，不加一个变量的话会有个编译警告
// Warning: ISO C++11 requires at least one argument for the "..." in a variadic
// macro
// https://stackoverflow.com/questions/11761703/overloading-macro-on-number-of-arguments
#define ASSERT(...) \
    __EXPAND(__VA_ARGS__, __ASSERT2, __ASSERT1, _UNUSED)(__VA_ARGS__)

#endif /* NDEBUG */
