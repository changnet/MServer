#ifndef __BUFFER_H__
#define __BUFFER_H__

#include "../global/global.h"
#include "../pool/ordered_pool.h"

/*
 *    +---------------------------------------------------------------+
 *    |    悬空区   |        数据区          |      空白buff区          |
 *    +---------------------------------------------------------------+
 * _buff          _pos                    _size
 *
 *收发缓冲区，要考虑游戏场景中的几个特殊情况：
 * 1.游戏的通信包一般都很小，reserved出现的概率很小。偶尔出现，memcpy的效率也是可以接受的
 * 2.缓冲区可能出现边读取边接收，边发送边写入的情况。因此，当前面的协议未读取完，又接收到新的
 *   协议，或者前面的数据未发送完，又写入新的数据，会造成前面一段缓冲区悬空，没法利用。旧框架
 *   的办法是每读取完或发送完一轮数据包，就用memmove把后面的数据往前面移动，这在粘包频繁出现
 *   并且包不完整的情况下效率很低(进程间的socket粘包很严重)。因此，我们忽略前面的悬空缓冲区，
 *   直到我们需要调整内存时，才用memmove移动内存。
 */

class buffer
{
public:
    buffer();
    ~buffer();

    static void purge() { allocator.purge(); }

    bool append( const void *data,uint32 len )
        __attribute__ ((warn_unused_result))
    {
        if ( !reserved( len ) ) return false;

        __append( data,len );    return true;
    }

    /* 减去缓冲区数据，此函数不处理缓冲区的数据 */
    inline void subtract( uint32 len )
    {
        _pos += len;
        assert( "buffer subtract",_size >= _pos && _len >= _pos );

        if ( _size == _pos ) _pos = _size = 0;
    }

    /* 增加数据区 */
    inline void increase( uint32 len )
    {
        _size += len;
        assert( "buffer increase",_size <= _len );
    }

    /* 重置 */
    inline void clear() { _pos = _size = 0; }
    /* 总大小 */
    inline uint32 length() const { return _len; }

    /* 数据区大小 */
    inline uint32 data_size() const { return _size - _pos; }
    /* 数据区指针 */
    inline char *data_pointer() const { return _buff + _pos; }

    /* 缓冲区大小 */
    inline uint32 buff_size() const { return _len - _size; }
    /* 缓冲区指针 */
    inline char *buff_pointer() const { return _buff + _size; }

    /* raw append data,but won't reserved */
    void __append( const void *data,const uint32 len )
    {
        assert( "buffer not reserved!",_len - _size >= len );
        memcpy( _buff + _size,data,len );       _size += len;
    }

    /* 设置缓冲区最大最小值 */
    void set_buffer_size( uint32 max,uint32 min )
    {
        _max_buff = max;
        _min_buff = min;
    }
public:
    /* 内存预分配：
     * @bytes : 要增长的字节数。默认为0,首次分配BUFFER_CHUNK，用完再按指数增长
     * @vsz   : 已往缓冲区中写入的字节数，但未增加_size偏移，主要用于自定义写入缓存
     */
    inline bool reserved( uint32 bytes = 0,uint32 vsz = 0 )
        __attribute__ ((warn_unused_result))
    {
        uint32 size = _size + vsz;
        if ( _len - size > bytes ) return true;/* 不能等于0,刚好用完也申请 */

        if ( _pos )    /* 解决悬空区 */
        {
            assert( "reserved memmove error",_size > _pos && size <= _len );
            memmove( _buff,_buff + _pos,size - _pos );
            _size -= _pos;
            _pos   = 0   ;

            return reserved( bytes,vsz );
        }

        assert( "buffer no min or max setting",_min_buff > 0 && _max_buff > 0 );

        uint32 new_len = _len  ? _len  : _min_buff;
        uint32 _bytes  = bytes ? bytes : BUFFER_CHUNK;
        while ( new_len - size < _bytes )
        {
            new_len *= 2;  /* 通用算法：指数增加 */
        }

        if ( new_len > _max_buff ) return false;

        /* 检验内在分配大小是否符合机制 */
        assert( "buffer chunk size error",0 == new_len%BUFFER_CHUNK );

        uint32 chunk_size = new_len >= BUFFER_LARGE ? 1 : BUFFER_CHUNK_SIZE;
        char *new_buff =
            allocator.ordered_malloc( new_len/BUFFER_CHUNK,chunk_size );

        /* 像STL一样把旧内存拷到新内存 */
        if ( size ) memcpy( new_buff,_buff,size );

        if ( _len ) allocator.ordered_free( _buff,_len/BUFFER_CHUNK );

        _buff = new_buff;
        _len  = new_len ;

        return      true;
    }
private:
    buffer( const buffer & );
    buffer &operator=( const buffer &);
private:
    char  *_buff;    /* 缓冲区指针 */
    uint32 _size;    /* 缓冲区已使用大小 */
    uint32 _len ;    /* 缓冲区总大小 */
    uint32 _pos ;    /* 悬空区大小 */

    uint32 _max_buff; /* 缓冲区最小值 */
    uint32 _min_buff; /* 缓冲区最大值 */
private:
    static class ordered_pool<BUFFER_CHUNK> allocator;
};

#endif /* __BUFFER_H__ */
