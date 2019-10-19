#pragma once

#include <cassert>

#define __ASSERT1(expr) assert(expr)
#define __ASSERT2(expr,why) \
     ((expr)								\
   ? __ASSERT_VOID_CAST (0)						\
   : __assert_fail (__STRING((why,expr)), __FILE__, __LINE__, __ASSERT_FUNCTION))

#define __EXPAND(_1, _2, NAME, ...) NAME
#define ASSERT(...) __EXPAND(__VA_ARGS__, __ASSERT2, __ASSERT1)(__VA_ARGS__)
