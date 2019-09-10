#pragma once

#include "../buffer.h"

/* socket input output control */
class io
{
public:
    typedef enum
    {
        IOT_NONE = 0,
        IOT_SSL  = 1,

        IOT_MAX
    }io_t;
public:
    virtual ~io();
    io( class buffer *recv,class buffer *send );

    /* 接收数据
     * 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
     * @byte: 接收的数据长度
     */
    virtual int32 recv( int32 &byte );
    /* 发送数据
     * 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
     * @byte: 发送的数据长度
     */
    virtual int32 send( int32 &byte );
    /* 准备接受状态
     * 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
     */
    virtual int32 init_accept( int32 fd ) { return _fd = fd; };
    /* 准备连接状态
     * 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
     */
    virtual int32 init_connect( int32 fd ) { return _fd = fd; };
protected:
    int32 _fd;
    class buffer *_recv;
    class buffer *_send;
};
