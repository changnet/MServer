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

/*
stream只做流协议解析
另一种方法是做lbuffer,那么read、write、reserver在buffer做就行了，不需要stream.注意设定一个最大buffer值
那么要把buffer改成纯buffer类，把recv、send放到socket，(这时又怎样访问buffer指针呢？buffer变量设计为public)

leventloop与socket之前的耦合过高，直接访问了socket的buffer、sending标志位，看能不能优化一下
把三级socket、lsocket、http_socket改为继承，设计并完成stream_socket
*/

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

#define DEFINE_READ_FUNCTION(type)                                     \
        inline int32 read_##type( char* const buffer,type &val ) \
        {                                                              \

            LDR( buffer,val,type );                           \
            return sizeof( type );
        }

#define DEFINE_WRITE_FUNCTION(type)                                     \
        inline int32 write_##type( char* const buffer,const type &val ) \
        {
            STR( buffer,val,type );
            return sizeof( type );
        }

        /* 具体实现每一个基本类型的操作
         * 不用模板是为了防止传入size_t这种32/64平台相关的类型
         * 或者发生隐式强制转换导致数据错误
         * 尤其是使用了reinterpret_cast
         */
        DEFINE_READ_FUNCTION(  int8  );
        DEFINE_READ_FUNCTION( uint8  );
        DEFINE_READ_FUNCTION(  int16 );
        DEFINE_READ_FUNCTION( uint16 );
        DEFINE_READ_FUNCTION(  int32 );
        DEFINE_READ_FUNCTION( uint32 );
        DEFINE_READ_FUNCTION(  int64 );
        DEFINE_READ_FUNCTION( uint64 );

        DEFINE_WRITE_FUNCTION(  int8  );
        DEFINE_WRITE_FUNCTION( uint8  );
        DEFINE_WRITE_FUNCTION(  int16 );
        DEFINE_WRITE_FUNCTION( uint16 );
        DEFINE_WRITE_FUNCTION(  int32 );
        DEFINE_WRITE_FUNCTION( uint32 );
        DEFINE_WRITE_FUNCTION(  int64 );
        DEFINE_WRITE_FUNCTION( uint64 );

#undef LDR
#undef STR
#undef DEFINE_READ_FUNCTION
#undef DEFINE_WRITE_FUNCTION

        /* 对string类型的操作 */
        inline int32 read_string( char* const buffer,const int32 len )
        {

        }
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

    typedef std::unordered_map< pair_key_t,struct protocol,pair_hash,pair_equal > unordered_map_t;
     unordered_map_t _protocol;
};

#endif /* __STREAM_H__ */
