#pragma once

#include <openssl/ssl.h>

#include "io.hpp"

class TlsCtx;

class SSLIO final: public IO
{
public:
    ~SSLIO();
    explicit SSLIO(TlsCtx *tls_ctx);

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
     * @brief 执行ssl握手（必须已初始化）
     */
    int32_t handshake(EVIO *w) override;
    /**
     * 执行初始化接受的连接
     * @return int32_t
     */
    virtual int32_t do_init_accept(EVIO *w) override;
    /**
     * 执行初始化连接
     * @return int32_t
     */
    virtual int32_t do_init_connect(EVIO *w) override;
    /**
     * @brief 设置ssl的alpn
     */
    virtual int32_t set_ssl_alpn(int32_t alpn) override;
    /**
     * @brief 设置ssl的sni(service name indicator)
     */
    virtual int32_t set_ssl_sni(const char *sni) override;
    /**
     * @brief 设置ssl的证书host
     */
    virtual int32_t set_ssl_cert_host(const char *host) override;
    /**
     * @brief 设置ssl的验证模式
     */
    virtual int32_t set_ssl_verify_mode(int32_t mode) override;

private:
    int32_t init_ssl_ctx(int32_t fd);

private:
    SSL *ssl_;
};
