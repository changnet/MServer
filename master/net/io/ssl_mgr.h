#ifndef __SSL_MGR_H__
#define __SSL_MGR_H__

// 最大8组ctx来适应不同版本的、不同连接，应该足够了
#define MAX_SSL_CTX  8

#include "../../global/global.h"

struct ssl_ctx;
class ssl_mgr
{
public:
    typedef enum
    {
        SSLV_2
    }sslv_t;
public:
    static void uninstance();
    static class ssl_mgr *instance();

    SSL_CTX *get_ssl_ctx();
    int32 new_ssl_ctx( sslv_t sslv,const char *cert_file,const char *key_file );
private:
    ssl_mgr();
    ~ssl_mgr();

    int32 _ctx_idx;
    ssl_ctx* _ssl_ctx[MAX_SSL_CTX];
    static class ssl_mgr *_ssl_mgr;
};

#endif /* __SSL_MGR_H__ */