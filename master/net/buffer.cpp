#include "buffer.h"

class ordered_pool<BUFFER_CHUNK> buffer::allocator;

buffer::buffer()
{
    _buff  = NULL;
    _size  = 0;
    _len   = 0;
    _pos   = 0;
}

buffer::~buffer()
{
    if ( _len )
    {
        allocator.ordered_free( _buff,_len/BUFFER_CHUNK );
    }

    _buff = NULL;
    _size = 0;
    _len  = 0;
    _pos  = 0;
}

/* 追加数据 */
void buffer::append( const char *data,uint32 len )
{
    reserved( len );
    memcpy( _buff + _size,data,len );
    _size += len;
}
