#include "io.h"

io::io()
{
}

io::~io()
{
}

int32 io::recv( int32 fd,class buffer &buff ) const
{
    if ( !buff.reserved() ) return -1; /* no more memory */

    assert( "socket recv buffer length <= 0",buff._len - buff._size > 0 );
    int32 len = ::read( fd,buff._buff + buff._size,buff._len - buff._size );
    if ( len > 0 )
    {
        buff._size += len;
    }
    return len;
}

int32 io::send( int32 fd,class buffer &buff ) const
{
    assert( "buf send without data",buff._size - buff._pos > 0 );

    int32 len = ::write( fd,buff._buff + buff._pos,buff._size - buff._pos );

    if ( len > 0 ) buff._pos += len;

    return len;
}
