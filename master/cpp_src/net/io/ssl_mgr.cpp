#include <openssl/ssl.h>
#include <openssl/err.h>

#include "ssl_mgr.h"

// SSL的错误码是按队列存放的，一次错误可以产生多个错误码
// 因此出错时，需要循环用ERR_get_error来清空错误码或者调用ERR_clear_error
#define SSL_ERROR(x)    \
    do{                                                     \
        ERROR(x " errno(%d:%s)",errno,strerror(errno));     \
        int32_t eno = 0;                                      \
        while ( 0 != (eno = ERR_get_error()) ) {            \
            ERROR( "    %s",ERR_error_string(eno,NULL) );   \
        }                                                   \
    }while(0)

int32_t ctx_passwd_cb( char *buf, int32_t size, int rwflag, void *u );

void delete_ssl_ctx( struct x_ssl_ctx &ssl_ctx )
{
    if ( ssl_ctx._ctx )
    {
        SSL_CTX_free( static_cast<SSL_CTX *>(ssl_ctx._ctx) );
    }
    if ( ssl_ctx._passwd )
    {
        delete []ssl_ctx._passwd;
    }

    ssl_ctx._ctx = NULL;
    ssl_ctx._passwd = NULL;
}

ssl_mgr::ssl_mgr()
{
    _ctx_idx = 0;
    memset( _ssl_ctx,0,sizeof(_ssl_ctx) );
}

ssl_mgr::~ssl_mgr()
{
    _ctx_idx = 0;
    for ( int32_t idx = 0;idx < MAX_SSL_CTX;idx ++ )
    {
        delete_ssl_ctx( _ssl_ctx[idx] );
    }
    memset( _ssl_ctx,0,sizeof(_ssl_ctx) );
}

void *ssl_mgr::get_ssl_ctx( int32_t idx )
{
    if ( idx < SSLV_NONE || idx >= _ctx_idx ) return NULL;

    return _ssl_ctx[idx]._ctx;
}

/* 这个函数会造成较多的内存检测问题
 * Use of uninitialised value of size 8
 * Conditional jump or move depends on uninitialised value(s)
 * https://www.mail-archive.com/openssl-users@openssl.org/msg45215.html
 */
int32_t ssl_mgr::new_ssl_ctx( sslv_t sslv,
    const char *cert_file,key_t keyt,const char *key_file,const char *passwd )
{
    if ( _ctx_idx >= MAX_SSL_CTX )
    {
        ERROR( "new_ssl_ctx:over max ctx storage" );
        return -1;
    }

    const SSL_METHOD *method = NULL;
    switch( sslv )
    {
// OPENSSL_VERSION_NUMBER定义在/usr/include/openssl/opensslv.h
#if OPENSSL_VERSION_NUMBER > 0x1000105fL
        case SSLV_TLS_GEN_AT : method = TLS_method(); break;
        case SSLV_TLS_SRV_AT : method = TLS_server_method(); break;
        case SSLV_TLS_CLT_AT : method = TLS_client_method(); break;
#endif
        // 新版本中无需指定版本号，用上面的自动协商·
        //  warning: ‘const SSL_METHOD* TLSv1_2_method()’ is deprecated
        // case SSLV_TLS_GEN_12 : method = TLSv1_2_method(); break;
        default :
            ERROR( "new_ssl_ctx:unknow ssl version" );
            return -1;
    }

    SSL_CTX *ctx = SSL_CTX_new( method );
    if ( !ctx )
    {
        SSL_ERROR( "new_ssl_ctx:can NOT create ssl content" );
        return -1;
    }

    struct x_ssl_ctx &ssl_ctx = _ssl_ctx[_ctx_idx ++];
    ssl_ctx._ctx = ctx;

    /* 建立ssl时，客户端的证书是在握手阶段由服务器发给客户端的
     * 因此单向认证的客户端使用的SSL_CTX不需要证书
     * 双向认证参考：SSL_CTX_set_verify()这个函数的用法。
     * 设置认证标识后，客户端便会把证书发给服务器，服务器会在
     * SSL_CTX_set_cert_verify_callback中收到回调，校验客户端的证书参数。
     * 一般情况下，如果客户端的域名不是固定的话，这个认证没什么意义的。
     * ssh那种是通过公钥、私钥来认证客户端的，不是证书。
     */
    if ( !cert_file || !key_file ) return 0;

    // 密码是从Lua传入的，需要拷贝一份，防止以后用到
    if ( passwd )
    {
        size_t size = strlen(passwd);
        ssl_ctx._passwd = new char[size + 1];
        memcpy( ssl_ctx._passwd,passwd,size );

        ssl_ctx._passwd[size] = 0;
    }

    // 加载pem格式的ca证书文件，暂不支持ASN1（SSL_FILETYPE_ASN1）格式
    // ASN1只支持一个文件一个证书，pem可以将多个证书放到同一个文件
    if ( SSL_CTX_use_certificate_chain_file( ctx,cert_file ) <= 0 )
    {
        SSL_CTX_free( ctx );
        SSL_ERROR( "new_ssl_ctx cert file" );
        return -1;
    }

    // 加载pem格式私钥
    // 如果key加了密码，则需要处理：SSL_CTX_set_default_passwd_cb
    // PS:ca证书是公开的，但其实也是可以加密码的，这时也要处理才能加载
    SSL_CTX_set_default_passwd_cb( ctx,ctx_passwd_cb );
    SSL_CTX_set_default_passwd_cb_userdata( ctx,ssl_ctx._passwd );

    int32_t ok = 0;
    switch( keyt )
    {
        case KEYT_GEN :
            ok = SSL_CTX_use_PrivateKey_file( ctx,key_file,SSL_FILETYPE_PEM );
            break;
        case KEYT_RSA :
            ok = SSL_CTX_use_RSAPrivateKey_file( ctx,key_file,SSL_FILETYPE_PEM );
            break;
        default :
            delete_ssl_ctx( ssl_ctx );
            ERROR( "invalid key type" );
            return -1;
    }
    if ( ok <= 0 )
    {
        delete_ssl_ctx( ssl_ctx );
        SSL_ERROR( "new_ssl_ctx key file" );
        return -1;
    }

    // 验证私钥是否和证书匹配
    if ( SSL_CTX_check_private_key( ctx ) <= 0 )
    {
        delete_ssl_ctx( ssl_ctx );
        SSL_ERROR( "new_ssl_ctx:certificate and private key not match" );
        return -1;
    }

    return _ctx_idx - 1;
}

// 返回密码数据
int32_t ctx_passwd_cb( char *buf, int32_t size, int rwflag, void *u )
{
    if ( !u ) return 0;

    snprintf( buf,size,"%s",(const char *)u );

    return strlen(buf);
}
