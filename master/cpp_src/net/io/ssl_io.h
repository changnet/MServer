#ifndef __SSL_IO_H__
#define __SSL_IO_H__

#include "io.h"

class ssl_io : public io
{
public:
    ~ssl_io();
    ssl_io( int32 ctx_idx,class buffer *recv,class buffer *send );

    /* 接收数据
     * 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
     * @byte: 接收的数据长度
     */
    int32 recv( int32 &byte );
    /* 发送数据
     * 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
     * @byte: 发送的数据长度
     */
    int32 send( int32 &byte );
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
