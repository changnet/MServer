#ifndef __TYPES_H__
#define __TYPES_H__

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

typedef uint8_t        uint8;
typedef int8_t          int8;
typedef uint16_t      uint16;
typedef int16_t        int16;
typedef uint32_t      uint32;
typedef int32_t        int32;
typedef uint64_t      uint64;
typedef int64_t        int64;

#define FMT64d "%" PRId64
#define FMT64u "%" PRIu64

/* avoid c-style string */
typedef std::string    string;

/* 需要使用hash map，但又希望能兼容旧版本时使用map_t */
#if __cplusplus < 201103L    /* -std=gnu99 */
    #include <map>
    #define map_t    std::map
#else                       /* if support C++ 2011 */
    #include <unordered_map>
    #define map_t    std::unordered_map
#endif

#if __cplusplus >= 201103L
    //#pragma message("base type bit check")
    static_assert( sizeof( int8 ) == 1,"type bit not support" );
    static_assert( sizeof(uint8 ) == 1,"type bit not support" );
    static_assert( sizeof( int16) == 2,"type bit not support" );
    static_assert( sizeof(uint16) == 2,"type bit not support" );
    static_assert( sizeof( int32) == 4,"type bit not support" );
    static_assert( sizeof(uint32) == 4,"type bit not support" );
    static_assert( sizeof( int64) == 8,"type bit not support" );
    static_assert( sizeof(uint64) == 8,"type bit not support" );
    static_assert( sizeof( float) == 4,"type bit not support" );
    static_assert( sizeof(double) == 8,"type bit not support" );
#endif

#endif    /* __TYPES_H__ */
