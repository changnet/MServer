#include <openssl/ssl.h>

#include "ssl_mgr.h"

// SSL_CTX是typedef，用这种方式来实现前置声明
struct ssl_ctx : public SSL_CTX {};

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
    const SSL_METHOD *method = NULL;
    switch( sslv )
    {
// OPENSSL_VERSION_NUMBER定义在/usr/include/openssl/opensslv.h
#if OPENSSL_VERSION_NUMBER > 0x1000105fL
        case SSLV_TLS_GEN_AT : method = TLS_method(); break;
        case SSLV_TLS_SRV_AT : method = TLS_server_method(); break;
        case SSLV_TLS_CLT_AT : method = TLS_client_method(); break;
#else
#endif
        default :
            ERROR( "new_ssl_ctx:unknow ssl version" );
            return -1;
    }
    struct ssl_ctx *ctx = 
        static_cast<struct ssl_ctx *>( SSL_CTX_new( method ) );
    if ( !ctx )
    {
        ERROR( "new_ssl_ctx:can NOT create ssl content" );
        return -1;
    }
    // 加载pem格式的ca证书文件，暂不支持ASN1（SSL_FILETYPE_ASN1）格式
    // ASN1只支持一个文件一个证书，pem可以将多个证书放到同一个文件
    if ( SSL_CTX_use_certificate_chain_file( ctx,cert_file ) <= 0 )
    {
        SSL_CTX_free( ctx );
        ERROR( "new_ssl_ctx:SSL_CTX_use_certificate_chain_file fail" );
        return -1;
    }

    // 加载pem格式私钥
    // 如果key加了密码，则需要处理：SSL_CTX_set_default_passwd_cb
    // PS:ca证书是公开的，但其实也是可以加密码的，这时也要处理才能加载
    if ( SSL_CTX_use_PrivateKey_file( ctx,key_file,SSL_FILETYPE_PEM ) <= 0 )
    {
        SSL_CTX_free( ctx );
        ERROR( "new_ssl_ctx:SSL_CTX_use_PrivateKey_file fail" );
        return -1;
    }

    // 验证私钥是否和证书匹配
    if ( SSL_CTX_check_private_key( ctx ) <= 0 )
    {
        SSL_CTX_free( ctx );
        ERROR( "new_ssl_ctx:certificate and private key not match" );
        return -1;
    }

    _ssl_ctx[_ctx_idx++] = ctx;
    return 0;
}
