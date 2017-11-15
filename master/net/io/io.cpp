#include "io.h"

io::io()
{
}

io::~io()
{
}

int32 io::recv()
{
    if ( !_recv.reserved() ) return -1; /* no more memory */

    assert( "socket recv buffer length <= 0",_recv._len - _recv._size > 0 );
    int32 len = ::read( 
        _w.fd,_recv._buff + _recv._size,_recv._len - _recv._size );
    if ( len > 0 )
    {
        _recv._size += len;
    }
    return len;
}

int32 io::send()
{
    assert( "buf send without data",_send._size - _send._pos > 0 );

    int32 len = ::write( 
        _w.fd,_send._buff + _send._pos,_send._size - _send._pos );
    if ( len > 0 )
    {
        _send._pos += len;
    }

    return len;
}

int32 io::accept()
{
    return ::accept(_w.fd,NULL,NULL);
}