#ifndef __BUFFER_H__
#define __BUFFER_H__

#include "../global/global.h"
#include "../pool/ordered_pool.h"

/* 收发缓冲区，要考虑游戏场景中的几个特殊情况：
 * 1.游戏的通信包一般都很小，reserved出现的概率很小。偶尔出现，memcpy的效率也是可以接受的
 * 2.缓冲区可能出现边读取边接收，边发送边写入的情况。因此，当前面的协议未读取完，又接收到新的
 *   协议，或者前面的数据未发送完，又写入新的数据，会造成前面一段缓冲区悬空，没法利用。旧框架
 *   的办法是每读取完或发送完一轮数据包，就用memmove把后面的数据往前面移动，这在粘包频繁出现
 *   并且包不完整的情况下效率很低(进程间的socket粘包很严重)。因此，我们忽略前面的悬空缓冲区，
 *   直到我们需要调整内存时，才用memmove移动内存。
 */

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
        inline type read_##type()                                      \
        {                                                              \
            if ( data_size() >= sizeof(type) )                         \
            {                                                          \
                /* not assert */                                       \
                ERROR( "read_"#type" overflow" );                      \
                return 0;                                              \
            }                                                          \
            type val = 0;                                              \
            LDR( _buff + _pos,val,type );                              \
            return val;                                                \
        }

#define DEFINE_WRITE_FUNCTION(type)                                     \
        inline int32 write_##type( const type &val )                    \
        {                                                               \
            reserved( sizeof(type) );                                   \
            STR( _buff + _pos,val,type );                               \
            _size += sizeof(type);                                      \
            return sizeof( type );                                      \
        }

class buffer_process;

class buffer
{
public:
    buffer();
    ~buffer();

    void append( const char *data,uint32 len );

    /* 从socket读取数据 */
    inline int32 recv( int32 fd )
    {
        reserved();
        int32 len = ::read( fd,_buff + _size,_len - _size );
        if ( len > 0 )
        {
            _size += len;
        }

        return len;
    }

    /* 发送数据 */
    inline int32 send( int32 fd )
    {
        assert( "buff send without data",_size - _pos > 0 );

        int32 len = ::write( fd,_buff + _pos,_size - _pos );
        if ( len > 0 )
            _pos += len;

        return len;
    }

    /* 清理缓冲区 */
    inline void clear()
    {
        _pos = _size = 0;
    }

    /* 有效数据大小 */
    inline uint32 data_size()
    {
        return _size - _pos;
    }

    /* 悬空区移动 */
    inline void moveon( int32 _mv )
    {
        _pos += _mv;
    }

    /* 数据区大小 */
    inline uint32 size()
    {
        return _size;
    }

    /* 总大小 */
    inline uint32 length()
    {
        return _len;
    }

    /* 悬空区大小 */
    inline uint32 empty_head_size()
    {
        return _pos;
    }

    /* 有效的缓冲区指针 */
    const char *buff_pointer()
    {
        return _buff + _pos;
    }

    friend class buffer_process;

    static class ordered_pool<BUFFER_CHUNK> allocator;
public:
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

    /* 对string类型的操作 */
    inline int32 read_string( char* const ptr,const int32 len )
    {
        uint16 str_len = read_uint16();
        if ( str_len <= 0 || len < str_len ) return -1;

        memcpy( ptr,_buff + _pos,str_len );
        return str_len;
    }
private:
    char  *_buff;    /* 缓冲区指针 */
    uint32 _size;    /* 缓冲区已使用大小 */
    uint32 _len;     /* 缓冲区总大小 */
    uint32 _pos;     /* 悬空区大小 */

    /* 内存扩展,处理两种情况：
     * 1.未知大小(从socket读取时)，默认首次分配BUFFER_CHUNK，用完再按指数增长
     * 2.已知大小(发送数据时)，指数增长到合适大小
     */
    inline void reserved( uint32 bytes = 0 )
    {
        assert( "max buffer overflow",BUFFER_MAX >= _len );
        if ( _len - _size > bytes ) /* 不能等于0,刚好用完也申请 */
            return;

        if ( _pos )    /* 解决悬空区 */
        {
            assert( "reserved memmove error",_size > _pos );
            memmove( _buff,_buff + _pos,_size - _pos );
            _size -= _pos;
            _pos   = 0;

            reserved( bytes );
        }

        uint32 new_len = _len  ? _len  : BUFFER_CHUNK;
        uint32 _bytes  = bytes ? bytes : BUFFER_CHUNK;
        while ( new_len - _size < _bytes )
        {
            new_len *= 2;  /* 通用算法：指数增加 */
        }

        /* 像STL一样把旧内存拷到新内存 */
        char *new_buff = allocator.ordered_malloc( new_len/BUFFER_CHUNK );

        if ( _size )
            memcpy( new_buff,_buff,_size );

        if ( _len )
            allocator.ordered_free( _buff,_len/BUFFER_CHUNK );

        _buff = new_buff;
        _len  = new_len;
    }
};

#undef LDR
#undef STR
#undef DEFINE_READ_FUNCTION
#undef DEFINE_WRITE_FUNCTION

#endif /* __BUFFER_H__ */
