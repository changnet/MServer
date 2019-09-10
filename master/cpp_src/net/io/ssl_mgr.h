#pragma once

// 最大8组ctx来适应不同版本的、不同连接，应该足够了
#define MAX_SSL_CTX  8

#include "../../global/global.h"

struct x_ssl_ctx
{
    void *_ctx;
    char *_passwd;
};

class ssl_mgr
{
public:
    // 自动协商版本号指在SSLv3, TLSv1, TLSv1.1 and TLSv1.2版本号中选择一个
    // SSL2、SSL3是deprecated的，仅作备用。尽量使用TLS高版本
    typedef enum
    {
        SSLV_NONE    = 0, // 无效
        SSLV_TLS_GEN_AT    = 1, // 通用类型(generic type)，可作服务器或客户端,自动协商
        SSLV_TLS_SRV_AT    = 2, // 服务器用，自动协商版本
        SSLV_TLS_CLT_AT    = 3, // 客户端用，自动协商版本
        // SSLV_TLS_GEN_12    = 4, // 通用类型，TLS 1.2 deprecated
        // SSLV_SSL_GEN_23 = 4, // 通用类型，SSL2、SSL3版本
        // SSLV_SSL_SRV_23 = 5, // 服务器用，SSL2、SSL3版本
        // SSLV_SSL_CLT_23 = 6, // 客户端用，SSL2、SSL3版本

        SSLV_MAX
    }sslv_t;

    // key 加密类型
    typedef enum
    {
        KEYT_NONE = 0,
        KEYT_GEN  = 1, // 通用类型，BEGIN PRIVATE KEY
        KEYT_RSA  = 2, // RSA，BEGIN RSA PRIVATE KEY

        KEYT_MAX
    }key_t;
public:
    ~ssl_mgr();
    explicit ssl_mgr();

    /* 获取一个SSL_CTX
     * 之所以不直接返回SSL_CTX类弄，是因为不想包含巨大的openssl/ssl.h头文件
     * SSL_CTX是一个typedef，不能前置声明
     */
    void *get_ssl_ctx( int32 idx );
    /* 创建一个ssl上下文
     * @sslv： ssl版本，见sslv_t枚举
     * @cert_file: ca证书文件路径
     * @key_file: 私钥文件路径
     */
    int32 new_ssl_ctx( sslv_t sslv,const char *cert_file,
        key_t keyt,const char *key_file,const char *passwd );
private:
    int32 _ctx_idx;
    struct x_ssl_ctx _ssl_ctx[MAX_SSL_CTX];
};
