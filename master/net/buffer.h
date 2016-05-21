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

    /* 当前缓冲区指针(包含虚拟数据) */
    char *virtual_buffer()
    {
        reserved();
        return _buff + _pos + _size + _vsz;
    }

    /* 更新冲区某个位置内存数据 */
    template< class T>
    inline void update_virtual_buffer( char *pos,T &val )
    {
        memcpy( pos,&val,sizeof(T) );
    }

    /* 清除虚拟数据 */
    inline void virtual_zero() { _vsz = 0; }

    /* 虚拟数据长度 */
    inline uint32 virtual_size() { return _vsz; }

    /* 虚拟数据转换为缓冲区数据 */
    inline void virtual_flush() { _size += _vsz; }

    friend class buffer_process;

    static class ordered_pool<BUFFER_CHUNK> allocator;
public:
    template< class T >
    int32 write( const T &val )
    {
        static int32 sz = sizeof(T);
        reserved( sz );

        STR( _buff + _size + _vsz,val,T );
        _vsz += sz;

        return sz;
    }

    /* 写入字符串或二进制 */
    inline int32 write( const char *ptr,const int32 len )
    {
        assert( "write_string illegal argument",ptr && len > 0 );

        uint16 str_len = static_cast<uint16>( len );
        assert( "string length over UINT16_MAX",str_len == len );

        write( str_len ); /* 先写入长度 */
        reserved( len );
        memcpy( _buff + _size + _vsz,ptr,len );
        _vsz += len;

        return len;
    }

    template < class T >
    int32 read( T &val,bool move = true )
    {
        if ( data_size() < sizeof(T) )
        {
            return -1;
        }

        LDR( _buff + _pos,val,T );

        if ( move ) _pos += sizeof(T);

        return sizeof(T);
    }

    /* 读取字符串
     * 如果参数ptr为NULL，则只返回字符串长度。
     * 字符串结束符0不会自动加上
     */
    // inline int32 read_string( char* const ptr = NULL,const int32 len = 0,
    //     bool move = true )
    // {
        // uint16 str_len = 0;
        //
        // if ( read( str_len,move ) < 0 ) return -1;
        // if ( !ptr ) return str_len;
        //
        // uint32 dsize = data_size();
        // if ( ( move ? dsize : dsize-sizeof(uint16) ) < str_len )
        // {
        //     return -1;
        // }
        //
        // if ( str_len <= 0 || len < str_len ) return -1;
        // memcpy( ptr,_buff + _pos,str_len );
        //
        // if ( move ) _pos += str_len;
        // return str_len;
    // }

    /* 取字符串地址并返回字符串长度
     * 注意取得的地址生命周期
     */
    // inline int32 read_string( char **ptr,bool move = true )
    // {
    //     int32 err = 0;
    //     uint16 str_len = read_uint16( &err,move );
    //     if ( err < 0 ) return err;
    //
    //     uint32 dsize = data_size();
    //     if ( ( move ? dsize : dsize-sizeof(uint16) ) < str_len )
    //     {
    //         return -1;
    //     }
    //
    //     *ptr = _buff + _pos;
    //     if ( move ) _pos += str_len;
    //
    //     return str_len;
    // }
private:
    char  *_buff;    /* 缓冲区指针 */
    uint32 _size;    /* 缓冲区已使用大小 */
    uint32 _len;     /* 缓冲区总大小 */
    uint32 _pos;     /* 悬空区大小 */
    uint32 _vsz;     /* 虚拟数据大小 */

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
