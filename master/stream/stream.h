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

#include "../global/global.h"

struct protocol
{
    typedef enum
    {
        NONE   =  0,
        INT8   =  1,
        UINT8  =  2,
        INT16  =  3,
        UINT16 =  4,
        INT32  =  5,
        UINT32 =  6,
        INT64  =  7,
        UINT64 =  8,
        STRING =  9,
        BINARY = 10,
        ARRAY  = 11
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
public:
    class io
    {
        explicit io( char *buffer,uint32 size )
            : _buffer(buffer),_size(size),_offset(0)
        {
            assert( "stream:illeage io buffer",_buffer && _size > 0 );
        }
        public:
#if defined(__i386__) || defined(__x86_64__)

/* LDR/STR 对应汇编指令LDR/STR */
/* !!! unaligned access,部分arm cpu不支持 !!! */
# define LDR(from,to,type) (to = (*reinterpret_cast<const type *>(from)))
# define STR(to,from,type) ((*reinterpret_cast<type *>(to)) = from)

#else

/* memcpy 在所有平台上都是安全的，但效率稍慢 */
# define LDR(from,to,type) (memcpy( &to,from,sizeof type ))
# define STR(to,from,type) (memcpy( to,&from,sizeof type ))

#endif
            inline io &operator >> ( int8 &val )
            {
                static uint32 offset = sizeof( int8 );
                if ( _offset + offset > _size ) /* overflow */
                {
                    assert( "stream:io read int8 overflow",false );
                    return *this;
                }

                LDR( _buffer + _offset,val,int8 );
                _offset += offset;

                return *this;
            }

            inline io &operator << ( int8 &val )
            {
                static uint32 offset = sizeof( int8 );
                if ( _offset + offset > _size )
                {
                    assert( "stream::io write int8 overflow",false );
                    return *this;
                }

                STR( _buffer + _offset,val,int8 );
                _offset += offset;

                return *this;
            }
#undef LDR
#undef STR
        private:
            /* 声明模板，但不实现。防止其他类型强转 */
            template <typename T> io& operator >> ( T& val );
            template <typename T> io& operator << ( T& val );

            char * const _buffer;
            uint32 const _size ;
            uint32 _offset;
    };
private:
    /* 对于制定协议，16+16=32bit已足够 */
    typedef std::pair<uint16,uint16> pair_key_t;
    struct pair_hash
    {
        uint32 operator () (const pair_key_t& pk) const
        {
            return (0xffff0000 & (pk.first << 16)) | (0x0000ffff & pk.second);
        }
    };

    struct pair_equal
    {
        bool operator () (const pair_key_t& a, const pair_key_t& b) const
        {
            return a.first == b.first && a.second == b.second;
        }
    };

    std::unordered_map< pair_key_t,struct protocol,pair_hash,pair_equal > _protocol;
};

#endif /* __STREAM_H__ */
