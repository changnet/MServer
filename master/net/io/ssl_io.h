#ifndef __SSL_IO_H__
#define __SSL_IO_H__

#include "io.h"

class ssl_io : public io
{
public:
    ~ssl_io();
    ssl_io( int32 ctx_idx,class buffer *recv,class buffer *send );

    /* 接收数据
     * 返回：< 0 错误(包括对方主动断开)，0 需要重试，> 0 成功读取的字节数
     */
    int32 recv();
    /* 发送数据
     * 返回：< 0 错误(包括对方主动断开)，0 成功，> 0 仍需要发送的字节数
     */
    int32 send();
    /* 准备接受状态
     */
    int32 init_accept( int32 fd );
    /* 准备连接状态
     */
    int32 init_connect( int32 fd );
private:
    int32 do_handshake();
    int32 init_ssl_ctx( int32 fd );
private:
    int32 _ctx_idx;
    void *_ssl_ctx; // SSL_CTX，不要在头文件包含ssl.h，编译会增加将近1M
    bool _handshake;
};

#endif /* __SSL_IO_H__ */
