#pragma once

#include <openssl/ssl.h>

#include "io.hpp"

class TlsCtx;

class SSLIO final: public IO
{
public:
    ~SSLIO();
    explicit SSLIO(int32_t conn_id, TlsCtx *tls_ctx, class Buffer *recv,
                   class Buffer *send);

    /* 接收数据（此函数在io线程执行）
     * 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
     * @byte: 接收的数据长度
     */
    int32_t recv(EVIO *w) override;
    /* 发送数据（此函数在io线程执行）
     * 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
     * @byte: 发送的数据长度
     */
    int32_t send(EVIO *w) override;
    /**
     * 执行初始化接受的连接
     * @return int32_t
     */
    virtual int32_t do_init_accept(int32_t fd) override;
    /**
     * 执行初始化连接
     * @return int32_t
     */
    virtual int32_t do_init_connect(int32_t fd) override;

private:
    int32_t do_handshake();
    int32_t init_ssl_ctx(int32_t fd);

private:
    SSL *_ssl;
    TlsCtx *_tls_ctx;
};
