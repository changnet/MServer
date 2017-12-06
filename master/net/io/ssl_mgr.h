#ifndef __SSL_MGR_H__
#define __SSL_MGR_H__

// 最大8组ctx来适应不同版本的、不同连接，应该足够了
#define MAX_SSL_CTX  8

#include "../../global/global.h"

struct ssl_ctx;
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
        SSLV_TLS_GEN_12    = 4, // 通用类型，TLS 1.2
        // SSLV_SSL_GEN_23 = 4, // 通用类型，SSL2、SSL3版本
        // SSLV_SSL_SRV_23 = 5, // 服务器用，SSL2、SSL3版本
        // SSLV_SSL_CLT_23 = 6, // 客户端用，SSL2、SSL3版本

        SSLV_MAX
    }sslv_t;
public:
    static void uninstance();
    static class ssl_mgr *instance();

    SSL_CTX *get_ssl_ctx();
    /* 创建一个ssl上下文
     * @sslv： ssl版本，见sslv_t枚举
     * @cert_file: ca证书文件路径
     * @key_file: 私钥文件路径
     */
    int32 new_ssl_ctx( sslv_t sslv,const char *cert_file,const char *key_file );
private:
    ssl_mgr();
    ~ssl_mgr();

    int32 _ctx_idx;
    ssl_ctx* _ssl_ctx[MAX_SSL_CTX];
    static class ssl_mgr *_ssl_mgr;
};

#endif /* __SSL_MGR_H__ */
