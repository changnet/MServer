#include <openssl/ssl.h>

#include "ssl_mgr.h"

// SSL_CTX是typedef，用这种方式来实现前置声明
struct ssl_ctx : public SSL_CTX
{
};

class ssl_mgr *ssl_mgr::_ssl_mgr = NULL;

class ssl_mgr *ssl_mgr::instance()
{
    if ( !_ssl_mgr )
    {
        _ssl_mgr = new ssl_mgr();
    }

    return _ssl_mgr;
}

void ssl_mgr::uninstance()
{
    delete _ssl_mgr;
    _ssl_mgr = NULL;
}

ssl_mgr::ssl_mgr()
{
    _ctx_idx = 0;
    memset( _ssl_ctx,0,sizeof(_ssl_ctx) );
}

ssl_mgr::~ssl_mgr()
{
    _ctx_idx = 0;
    for ( int32 idx = 0;idx < MAX_SSL_CTX;idx ++ )
    {
        SSL_CTX_free( _ssl_ctx[idx] );
    }
    memset( _ssl_ctx,0,sizeof(_ssl_ctx) );
}

int32 ssl_mgr::new_ssl_ctx(
    sslv_t sslv,const char *cert_file,const char *key_file )
{
    return 0;
}