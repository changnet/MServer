#pragma once

#include "io.h"

class SSLIO : public IO
{
public:
    ~SSLIO();
    explicit SSLIO(int32_t ssl_id, class Buffer *recv, class Buffer *send);

    /* 接收数据
     * 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
     * @byte: 接收的数据长度
     */
    int32_t recv(int32_t &byte);
    /* 发送数据
     * 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
     * @byte: 发送的数据长度
     */
    int32_t send(int32_t &byte);
    /* 准备接受状态
     */
    int32_t init_accept(int32_t fd);
    /* 准备连接状态
     */
    int32_t init_connect(int32_t fd);

private:
    int32_t do_handshake();
    int32_t init_ssl_ctx(int32_t fd);

private:
    int32_t _ssl_id;
    SSL *_ssl_ctx; // SSL_CTX，不要在头文件包含ssl.h，编译会增加将近1M
};
