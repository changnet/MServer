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
    virtual ~io();
    io( class buffer *recv,class buffer *send );

    /* 接收数据
     * 返回：< 0 错误(包括对方主动断开)，0 需要重试，> 0 成功读取的字节数
     */
    virtual int32 recv();
    /* 发送数据
     * 返回：< 0 错误(包括对方主动断开)，0 成功，> 0 仍需要发送的字节数
     */
    virtual int32 send();
    /* 准备接受状态
     */
    virtual int32 init_accept() { return 0; };
    /* 准备连接状态
     */
    virtual int32 init_connect() { return 0; };

    inline void set_fd( int32 fd ) { _fd = fd; }
protected:
    int32 _fd;
    class buffer *_recv;
    class buffer *_send;
};

#endif /* __IO_H__ */
