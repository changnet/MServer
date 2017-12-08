#ifndef __IO_H__
#define __IO_H__

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
    io();
    virtual ~io();

    /* 接收数据
     * 返回：< 0 错误(包括对方主动断开)，0 需要重试，> 0 成功读取的字节数
     */
    virtual int32 recv( int32 fd,class buffer &buff );
    /* 发送数据
     * 返回：< 0 错误(包括对方主动断开)，0 成功，> 0 仍需要发送的字节数
     */
    virtual int32 send( int32 fd,class buffer &buff );
};

#endif /* __IO_H__ */
