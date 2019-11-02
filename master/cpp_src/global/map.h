#pragma once

#include <cstdio>
#include <cstring>

//--------------------------------------------------------------------------
// MurmurHash2, by Austin Appleby
// Note - This code makes a few assumptions about how your machine behaves -

// 1. We can read a 4-byte value from any address without crashing
// 2. sizeof(int) == 4

// And it has a few limitations -

// 1. It will not work incrementally.
// 2. It will not produce the same results on little-endian and big-endian
//    machines.

static inline
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

struct c_string
{
    size_t _length;
    unsigned int _hash;
    const char *_raw_ctx;
    c_string(const char *ctx)
    {
        _raw_ctx = ctx;
        _length = strlen(ctx);
        _hash = MurmurHash2(ctx,_length,static_cast<size_t>(0xc70f6907UL));
    }
};

/* 自定义类型hash也可以放到std::hash中，暂时不这样做
 * https://en.cppreference.com/w/cpp/utility/hash
 */

/* the default hash function in libstdc++:MurmurHashUnaligned2
 * https://sites.google.com/site/murmurhash/ by Austin Appleby
 * other hash function(djb2,sdbm) http://www.cse.yorku.ca/~oz/hash.html */
struct hash_c_string
{
    size_t operator()(const c_string &cs) const
    {
        return cs._hash;
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
struct equal_c_string
{
    bool operator()(const c_string &a, const c_string &b) const
    {
        return 0 == std::strcmp(a._raw_ctx, b._raw_ctx);
    }
};

/* 需要使用hash map，但又希望能兼容旧版本时使用StdMap */
#if __cplusplus < 201103L    /* -std=gnu99 */
    #include <map>
    #define StdMap    std::map
    #define const_char_map_t(T) std::map<const char *,T,cmp_const_char>
#else    /* if support C++ 2011 */
    #include <unordered_map>
    #define StdMap    std::unordered_map
    // TODO:template<class T> using const_char_map_t = ...，但03版本不支持
    #define const_char_map_t(T)    \
        std::unordered_map<const c_string,T,hash_c_string,equal_c_string>
#endif
