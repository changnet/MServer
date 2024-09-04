#pragma once

#include <openssl/ssl.h>
#include "global/global.hpp"

// Transport Layer Security (TLS)数据，用于https和wss
class TlsCtx
{
public:
    TlsCtx();
    ~TlsCtx();

    static void library_end();
    static void library_init();

    /**
     * @brief 打印ssl错误信息
     * @param what 来源信息
     * @param e ssl错误码
     */
    static void dump_error(const char *what, int32_t e = 0);

    // 打印x509证书信息
    static void dump_x509(const SSL *ctx);

    /* 初始化一个ssl上下文
     * @cert_file: ca证书文件路径
     * @key_file: 私钥文件路径
     * @passwd: 私钥文件密码，如果无密码则为null
     */
    int32_t init(const char *cert_file, const char *key_file,
                        const char *passwd, const char *ca);

private:
    SSL_CTX *_ctx;
    char _passwd[256];
};