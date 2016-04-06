#ifndef __STREAM_H__
#define __STREAM_H__

#if(__cplusplus >= 201103L)
# include <unordered_map>
#else
# include <tr1/unordered_map>
namespace std
{
    using std::tr1::unordered_map;
}
#endif

#include "../global/global/h"

struct protocol
{
    typedef enum
    {
        PT_NONE   = 0,
        PT_INT8   = 1,
        PT_UINT8  = 2,
        PT_INT16  = 3,
        PT_UINT16 = 4,
        PT_INT32  = 5,
        PT_UINT32 = 6,
        PT_INT64  = 7,
        PT_UINT64 = 8,
        PT_STRING = 9,
        PT_ARRAY  = 10
    } protocol_t;

    protocol_t type;
    struct protocol *next;
    struct protocol *child;
};

class stream
{
public:
    int32 protocol_end();
    int32 protocol_begin( int32 mod,int32 func );

    int32 tag_int8 ( const char *key );
    int32 tag_int16( const char *key );
    int32 tag_int32( const char *key );
    int32 tag_int64( const char *key );

    int32 tag_uint8 ( const char *key );
    int32 tag_uint16( const char *key );
    int32 tag_uint32( const char *key );
    int32 tag_uint64( const char *key );

    int32 tag_string( const char * key );

    int32 tag_array_end();
    int32 tag_array_begin( const char *key );
private:
    std::map< std::pair<int32,int32>,struct protocol > _protocol;
};

#endif /* __STREAM_H__ */
