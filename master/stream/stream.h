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
        explicit io( const char *buffer,uint32 size )
            : _buffer(buffer),_size(size),_offset(0)
        {
            assert( "stream:illeage io buffer",_buffer && _size > 0 );
        }
        public:
#if defined(__i386__) || defined(__x86_64__)

/* LDR/STR 对应汇编指令LDR/STR */
# define LDR(_dest,_src,_t) _dest = (*reinterpret_cast<const _t *>(_p))

#else

# define LDR(_dest,_src,_t) memcpy( &_dest,_src,sizeof t )

#endif
            inline operator_t &operator >> ( int8 &val )
            {
                static uint32 offset = sizeof( int8 );
                if ( _offset + offset > _size ) /* overflow */
                {
                    assert( "stream:io read int8 overflow",false );
                    return *this;
                }

                /* val = *reinterpret_cast<const int8*>( buffer + _offset );
                 * 使用reinterpret_cast速度更快，但可能会出现内存对齐问题，尤其是
                 * 在编译时指定了对齐的情况下，类型强制转换后导致编译器
                 * memcpy 在O3优化下速度与reinterpret_cast基本相当
                 */
                memcpy( &val,buffer + _offset,offset );
                _offset += offset;

                return *this;
            }

            inline operator_t &operator << ( int8 &val )
            {
                static uint32 offset = sizeof( int8 );
                if ( _offset + offset > _size )
                {
                    assert( "stream::io write int8 overflow",false );
                    return *this;
                }


            }
#undef LDR
#undef STR
        private:
            /* 提供一个声明的模板防止未知类型做隐式转换 */
            template <typename T> operator_t& operator >> ( T& val );
            template <typename T> operator_t& operator << ( T& val );

            const char *_buffer;
            const uint32 _size ;
            uint32 _offset;
    };
private:
    std::map< std::pair<int32,int32>,struct protocol > _protocol;
};

#endif /* __STREAM_H__ */
