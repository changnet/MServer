#pragma once

/*
stdint.h
Including this file is the "minimum requirement" if you want to work with the
specified-width integer types of C99 (i.e. "int32_t", "uint16_t" etc.). If you
include this file, you will get the definitions of these types, so that you
will be able to use these types in declarations of variables and functions and
do operations with these datatypes.

inttypes.h
If you include this file, you will get everything that stdint.h provides
(because inttypes.h includes stdint.h), but you will also get facilities for
doing printf and scanf (and "fprintf, "fscanf", and so on.) with these types in
a portable way. For example, you will get the "PRIu16" macro so that you can
printf an uint16_t integer.
*/

#ifdef NULL
#undef NULL        /* in case <stdio.h> has defined it. or stddef.h */
#endif

#include<cassert>

#if __cplusplus < 201103L    /* <C++11 */
    #include <inttypes.h>
    #define NULL    0
#else                       /* if support C++ 2011 */
    #include <cinttypes>
    #define NULL    nullptr
#endif

#include "map.h" // custom map

typedef uint8_t        uint8_t;
typedef int8_t          int8_t;
typedef uint16_t      uint16_t;
typedef int16_t        int16_t;
typedef uint32_t      uint32_t;
typedef int32_t        int32_t;
typedef uint64_t      uint64_t;
typedef int64_t        int64_t;

#define FMT64d "%" PRId64
#define FMT64u "%" PRIu64

/* to avoid confuse,do NOT use c-style string,use char * instead */
#undef string
// typedef std::string    string;

#if __cplusplus >= 201103L
    //#pragma message("base type bit check")
    static_assert( sizeof( int8_t ) == 1,"type bit not support" );
    static_assert( sizeof(uint8_t ) == 1,"type bit not support" );
    static_assert( sizeof( int16_t) == 2,"type bit not support" );
    static_assert( sizeof(uint16_t) == 2,"type bit not support" );
    static_assert( sizeof( int32_t) == 4,"type bit not support" );
    static_assert( sizeof(uint32_t) == 4,"type bit not support" );
    static_assert( sizeof( int64_t) == 8,"type bit not support" );
    static_assert( sizeof(uint64_t) == 8,"type bit not support" );
    static_assert( sizeof( float) == 4,"type bit not support" );
    static_assert( sizeof(double) == 8,"type bit not support" );
#endif
