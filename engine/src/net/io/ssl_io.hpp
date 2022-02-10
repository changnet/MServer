#pragma once

#include <openssl/ssl.h>

#include "io.hpp"

class SSLIO final: public IO
{
public:
    ~SSLIO();
    explicit SSLIO(uint32_t conn_id, int32_t ssl_id, class Buffer *recv,
                   class Buffer *send);

    /* 接收数据
     * 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
     * @byte: 接收的数据长度
     */
    IOStatus recv() override;
    /* 发送数据
     * 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
     * @byte: 发送的数据长度
     */
    IOStatus send() override;
    /**
     * 发起初始化接受的连接
     * @return 是否需要异步执行action事件
     */
    int32_t init_accept(int32_t fd) override;
    /**
     * 发起初始化连接
     * @return 是否需要异步执行action事件
     */
    int32_t init_connect(int32_t fd) override;
    /**
     * 执行初始化接受的连接
     * @return IOStatus
     */
    virtual IOStatus do_init_accept() override;
    /**
     * 执行初始化连接
     * @return IOStatus
     */
    virtual IOStatus do_init_connect() override;
    /**
     * @brief init_accept、init_connect是否执行完
     */
    bool is_ready() const override;

private:
    IO::IOStatus do_handshake();
    int32_t init_ssl_ctx();

private:
    int32_t _ssl_id;
    SSL *_ssl_ctx;
};
