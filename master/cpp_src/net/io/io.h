#pragma once

#include "../buffer.h"

/* socket input output control */
class IO
{
public:
    typedef enum
    {
        IOT_NONE = 0,
        IOT_SSL  = 1,

        IOT_MAX
    } IOT;

public:
    virtual ~IO();
    IO(class Buffer *recv, class Buffer *send);

    /* 接收数据
     * 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
     * @byte: 接收的数据长度
     */
    virtual int32_t recv(int32_t &byte);
    /* 发送数据
     * 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
     * @byte: 发送的数据长度
     */
    virtual int32_t send(int32_t &byte);
    /* 准备接受状态
     * 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
     */
    virtual int32_t init_accept(int32_t fd) { return _fd = fd; }
    /* 准备连接状态
     * 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
     */
    virtual int32_t init_connect(int32_t fd) { return _fd = fd; }

protected:
    int32_t _fd;
    class Buffer *_recv;
    class Buffer *_send;
};
