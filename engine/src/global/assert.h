#pragma once

#include <cassert>

#define __ASSERT1(expr) assert(expr)

// __ASSERT_VOID_CAST is linux implement
#ifdef __ASSERT_VOID_CAST
#define __ASSERT2(expr, why)                                           \
    ((expr) ? __ASSERT_VOID_CAST(0)                                    \
            : __assert_fail(__STRING((why, expr)), __FILE__, __LINE__, \
                            __ASSERT_FUNCTION))
#else
#define __ASSERT2(expr, why) \
    (void) \
    ((!!(expr)) || \
    (_wassert(_CRT_WIDE(#why","#expr),_CRT_WIDE(__FILE__),__LINE__),0))
#endif

#define __EXPAND(_1, _2, NAME, ...) NAME
// _UNUSED avoid warning
// Warning: ISO C++11 requires at least one argument for the "..." in a variadic
// macro
// https://stackoverflow.com/questions/11761703/overloading-macro-on-number-of-arguments

// ASSERT(false) same as assert(false)
// ASSERT(false, "something wrong") if you prefer add some message to it
#define ASSERT(...) \
    __EXPAND(__VA_ARGS__, __ASSERT2, __ASSERT1, _UNUSED)(__VA_ARGS__)
