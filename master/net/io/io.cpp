#include "io.h"

io::io()
{
}

io::~io()
{
}

// 返回：< 0 错误(包括对方主动断开)，0 需要重试，> 0 成功读取的字节数
int32 io::recv( int32 fd,class buffer &buff )
{
    if ( !buff.reserved() ) return -1; /* no more memory */

    assert( "socket recv buffer length <= 0",buff._len - buff._size > 0 );
    int32 len = ::read( fd,buff._buff + buff._size,buff._len - buff._size );
    if ( expect_true(len > 0) )
    {
        buff._size += len;
        return len;
    }

    if ( 0 == len ) return -1; // 对方主动断开

    /* error happen */
    if ( errno != EAGAIN && errno != EWOULDBLOCK )
    {
        ERROR( "io send:%s",strerror(errno) );
        return -1;
    }

    return 0; // 重试
}

// 返回：< 0 错误(包括对方主动断开)，0 成功，> 0 仍需要发送的字节数
int32 io::send( int32 fd,class buffer &buff )
{
    size_t bytes = buff.data_size();
    assert( "io send without data",bytes > 0 );

    int32 len = ::write( fd,buff.data(),bytes );

    if ( expect_true(len > 0) )
    {
        buff.subtract( len );
        return ((size_t)len) == bytes ? 0 : bytes - len;
    }

    if ( 0 == len ) return -1;// 对方主动断开

    /* error happen */
    if ( errno != EAGAIN && errno != EWOULDBLOCK )
    {
        ERROR( "io send:%s",strerror(errno) );
        return -1;
    }

    /* need to try again */
    return bytes;
}
