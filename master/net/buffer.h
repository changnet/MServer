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

class buffer
{
public:
    buffer();
    ~buffer();

    void append( const char *data,uint32 len );

    /* 清理缓冲区 */
    inline void clear() { _pos = _size = 0; }

    /* 有效数据大小 */
    inline uint32 data_size() const { return _size - _pos; }

    /* 总大小 */
    inline uint32 length() const { return _len; }

    /* 有效的缓冲区指针 */
    operator char *() { return _buff + _pos; }
public:
    /* 内存扩展,处理两种情况：
     * 1.未知大小(从socket读取时)，默认首次分配BUFFER_CHUNK，用完再按指数增长
     * 2.已知大小(发送数据时)，指数增长到合适大小
     */
    inline void reserved( uint32 bytes = 0,uint32 vsz = 0 )
    {
        assert( "max buffer overflow",BUFFER_MAX >= _len );

        uint32 size = _size + vsz;
        if ( _len - size > bytes ) /* 不能等于0,刚好用完也申请 */
            return;

        if ( _pos )    /* 解决悬空区 */
        {
            assert( "reserved memmove error",_size > _pos && size <= _len );
            memmove( _buff,_buff + _pos,size - _pos );
            _size -= _pos;
            _pos   = 0;

            reserved( bytes,vsz );
            return;
        }

        uint32 new_len = _len  ? _len  : BUFFER_CHUNK;
        uint32 _bytes  = bytes ? bytes : BUFFER_CHUNK;
        while ( new_len - size < _bytes )
        {
            new_len *= 2;  /* 通用算法：指数增加 */
        }

        /* 像STL一样把旧内存拷到新内存 */
        char *new_buff = allocator.ordered_malloc( new_len/BUFFER_CHUNK );

        if ( size ) memcpy( new_buff,_buff,size );

        if ( _len ) allocator.ordered_free( _buff,_len/BUFFER_CHUNK );

        _buff = new_buff;
        _len  = new_len;
    }
public:
    char  *_buff;    /* 缓冲区指针 */
    uint32 _size;    /* 缓冲区已使用大小 */
    uint32 _len;     /* 缓冲区总大小 */
    uint32 _pos;     /* 悬空区大小 */

    static class ordered_pool<BUFFER_CHUNK> allocator;
};

#endif /* __BUFFER_H__ */
