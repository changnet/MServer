#include "buffer.h"

// 临时连续缓冲区
static const uint32 continuous_size = MAX_PACKET_LEN;
static char continuous_ctx[continuous_size] = { 0 };

buffer::buffer()
{
    _chunk_size = 0; // 已申请chunk数量

    _chunk_max = 8; // 允许申请chunk的最大数量
    // 默认单个chunk的缓冲区大小
    _chunk_ctx_size = (MAX_PACKET_LEN / BUFFER_CHUNK + 1) * BUFFER_CHUNK ;

    // TODO:服务器应该需要预先分配比较大的缓冲区而不是默认值
    _front = _back = new_chunk();
}

buffer::~buffer()
{
    while (_front)
    {
        chunk_t *tmp = _front;
        _front = _front->_next;

        del_chunk( tmp );
    }

    _front = _back = NULL;
}

// 清空所有内容
// 删除多余的chunk只保留一个
void buffer::clear()
{
    while (_front->_next)
    {
        chunk_t *tmp = _front;
        _front = _front->_next;

        del_chunk( tmp );
    }

    _front->clear();
    assert( "buffer link corruption", _back == _front );
}

// 添加数据
void buffer::append( const void *raw_data,const uint32 len )
{
    const char *data = reinterpret_cast<const char *>( raw_data );
    uint32 append_sz = 0;
    do
    {
        if ( !reserved() )
        {
            FATAL( "buffer append reserved fail" );
            return;
        }

        uint32 space = _back->space_size();

        uint32 size = MATH_MIN( space,len - append_sz );
        _back->append( data + append_sz,size );

        append_sz += size;

        // 大多数情况下，一次应该可以添加完数据
        // 如果不能，考虑调整单个chunk的大小，否则影响效率
    }while ( expect_false(append_sz < len) );
}

// 删除数据
void buffer::remove( uint32 len )
{
    uint32 remove_sz = 0;
    do
    {
        uint32 used = _front->used_size();

        // 这个chunk还有其他数据
        if ( used > len )
        {
            _front->remove( len );
            break;
        }

        // 这个chunk只剩下这个数据包
        if ( used == len )
        {
            chunk_t *next = _front->_next;
            if ( next )
            {
                // 还有下一个chunk，则指向下一个chunk
                del_chunk( _front );
                _front = next;
            }
            else
            {
                _front->clear(); // 在无数据的时候重重置缓冲区
            }
            break;
        }

        // 这个数据包分布在多个chunk，一个个删
        chunk_t *tmp = _front;

        _front = _front->_next;
        assert( "no more chunk to remove",_front );

        del_chunk( tmp );
        remove_sz += used;
    } while( true );
}

/* 检测指定长度的有效数据是否在连续内存，不在的话要合并成连续内存
 * protobuf这些都要求内存在连续缓冲区才能解析
 * TODO:这是采用这种设计缺点之二
 */
const char *buffer::check_used_ctx( uint32 len )
{
    // 大多数情况下，是在同一个chunk的，如果不是，调整下chunk的大小，否则影响效率
    if ( expect_true( _front->used_size() >= len ) )
    {
        return _front->used_ctx();
    }

    // 前期用来检测二次拷贝出现的情况，确认没问题这个可以去掉
    PRINTF( "using continuous buffer:%d",len );

    uint32 used = 0;
    const chunk_t *next = _front;

    do
    {
        uint32 next_used = next->used_size();
        assert( "continuous buffer overflow !!!",
            used + next_used <= continuous_size );

        memcpy( continuous_ctx + used,
            next->used_ctx(),MATH_MIN( len - used,next_used ) );

        used += next_used;
        next = next->_next;
    } while ( next && used < len );

    assert( "check_used_ctx fail", used == len );
    return continuous_ctx;
}

const char *buffer::check_all_used_ctx( uint32 &len )
{
    if ( expect_true( !_front->_next ) )
    {
        len = _front->used_size();
        return _front->used_ctx();
    }

    assert("检测大小",false); // 改名成 to_continuous_ctx

    uint32 used = 0;
    const chunk_t *next = _front;

    do
    {
        uint32 next_used = next->used_size();
        assert( "continuous buffer overflow !!!",
            used + next_used <= continuous_size );

        memcpy( continuous_ctx + used,next->used_ctx(),next_used );

        used += next_used;
        next = next->_next;
    } while ( next );

    len = used;
    assert( "check_used_ctx fail", used > 0 );

    // 前期用来检测二次拷贝出现的情况，确认没问题这个可以去掉
    PRINTF( "using all continuous buffer:%d",len );

    return continuous_ctx;
}
