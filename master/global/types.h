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

#if __cplusplus < 201103L    /* -std=gnu99 */
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

/* avoid c-style string */
typedef std::string    string;

#endif    /* __TYPES_H__ */
