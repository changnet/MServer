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

/* to avoid confuse,do NOT use c-style string,use char * instead */
typedef std::string    string;

/* 自定义类型hash也可以放到std::hash中，暂时不这样做
 * https://en.cppreference.com/w/cpp/utility/hash
 */

/* the default hash function in libstdc++:MurmurHashUnaligned2
 * https://sites.google.com/site/murmurhash/ by Austin Appleby
 * other hash function(djb2,sdbm) http://www.cse.yorku.ca/~oz/hash.html */
struct hash_const_char
{
    //--------------------------------------------------------------------------
    // MurmurHash2, by Austin Appleby
    // Note - This code makes a few assumptions about how your machine behaves -

    // 1. We can read a 4-byte value from any address without crashing
    // 2. sizeof(int) == 4

    // And it has a few limitations -

    // 1. It will not work incrementally.
    // 2. It will not produce the same results on little-endian and big-endian
    //    machines.

    static
    unsigned int MurmurHash2 ( const void * key, int len, unsigned int seed )
    {
        // 'm' and 'r' are mixing constants generated offline.
        // They're not really 'magic', they just happen to work well.

        const unsigned int m = 0x5bd1e995;
        const int r = 24;

        // Initialize the hash to a 'random' value

        unsigned int h = seed ^ len;

        // Mix 4 bytes at a time into the hash

        const unsigned char * data = (const unsigned char *)key;

        while(len >= 4)
        {
            unsigned int k = *(unsigned int *)data;

            k *= m; 
            k ^= k >> r; 
            k *= m; 

            h *= m; 
            h ^= k;

            data += 4;
            len -= 4;
        }
    
        // Handle the last few bytes of the input array

        switch(len)
        {
        case 3: h ^= data[2] << 16;
        case 2: h ^= data[1] << 8;
        case 1: h ^= data[0];
                h *= m;
        };

        // Do a few final mixes of the hash to ensure the last few
        // bytes are well-incorporated.

        h ^= h >> 13;
        h *= m;
        h ^= h >> 15;

        return h;
    } 

    size_t operator()(const char * str) const
    {
        // the default hash for std::string in libstdc++
        return MurmurHash2(str,strlen(str),static_cast<size_t>(0xc70f6907UL));
    }
};

/* compare function for const char* */
struct cmp_const_char
{
    bool operator()(const char *a, const char *b) const
    {
        return std::strcmp(a, b) < 0;
    }
};

/* compare function for const char* */
struct equal_const_char
{
    bool operator()(const char *a, const char *b) const
    {
        return 0 == std::strcmp(a, b);
    }
};

/* 需要使用hash map，但又希望能兼容旧版本时使用map_t */
#if __cplusplus < 201103L    /* -std=gnu99 */
    #include <map>
    #define map_t    std::map
    #define const_char_map_t(T) std::map<const char *,T,cmp_const_char>
#else    /* if support C++ 2011 */
    #include <unordered_map>
    #define map_t    std::unordered_map
    // TODO:template<class T> using const_char_map_t = ...，但03版本不支持
    #define const_char_map_t(T)    \
        std::unordered_map<const char*,T,hash_const_char,equal_const_char>
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
