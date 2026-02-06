#include <openssl/err.h>
#include <openssl/ssl.h>

#include "net/net_compat.hpp"
#include "tls_ctx.hpp"


void TlsCtx::library_end()
{
    /* The OPENSSL_cleanup() function deinitialises OpenSSL (both libcrypto and
     * libssl). All resources allocated by OpenSSL are freed. Typically there
     * should be no need to call this function directly as it is initiated
     * automatically on application exit. This is done via the standard C
     * library atexit() function. In the event that the application will close
     * in a manner that will not call the registered atexit() handlers then the
     * application should call OPENSSL_cleanup() directly. Developers of
     * libraries using OpenSSL are discouraged from calling this function and
     * should instead, typically, rely on auto-deinitialisation. This is to
     * avoid error conditions where both an application and a library it depends
     * on both use OpenSSL, and the library deinitialises it before the
     * application has finished using it.
     */

    // OPENSSL_cleanup();

}
void TlsCtx::library_init()
{
/* sha1、base64等库需要用到的
 * mongo c driver、mysql c connector等第三方库可能已初始化了ssl
 * ssl初始化是不可重入的。在初始化期间不要再调用任何相关的ssl函数
 * ssl可以初始化多次，在openssl\crypto\init.c中通过RUN_ONCE来控制
 */

// OPENSSL_VERSION_NUMBER定义在/usr/include/openssl/opensslv.h
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    SSL_library_init();
#else
    OPENSSL_init_ssl(0, nullptr);
#endif

    // OpenSSL now loads error strings automatically so these functions are not needed.
    // www.openssl.org/docs/manmaster/man7/migration_guide.html
    // ERR_load_BIO_strings();

    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
}

void TlsCtx::dump_error(const char *what, int32_t e)
{
    // SSL的错误码是按队列存放的，一次错误可以产生多个错误码
    // 因此出错时，需要循环用ERR_get_error来清空错误码或者调用ERR_clear_error

    int32_t net_e = netcompat::errorno();
    PLOG("%s ssl error(%d) errno(%d:%s)", what, e, net_e, netcompat::strerror(net_e));

    unsigned long eno = 0;
    while (0 != (eno = ERR_get_error()))
    {
        PLOG("    %s", ERR_error_string(eno, NULL));
        // PLOG("    lib:%s", ERR_lib_error_string(eno));
        // PLOG("    reason:%s", ERR_reason_error_string(eno));
    }
}

void TlsCtx::dump_x509(const SSL *ctx)
{
    X509 *cert = SSL_get_peer_certificate(static_cast<const SSL *>(ctx));
    if (cert != NULL)
    {
        char buf[256];

        // 证书拥有者
        X509_NAME_oneline(X509_get_subject_name(cert), buf, sizeof(buf));
        PLOG("x509 subject name: %s", buf);

        // 颁发者
        X509_NAME_oneline(X509_get_issuer_name(cert), buf, sizeof(buf));
        PLOG("x509 issuer name: %s", buf);

        X509_free(cert);
    }
}

TlsCtx::TlsCtx()
{
    ctx_ = nullptr;
    passwd_[0] = 0;
}

TlsCtx::~TlsCtx()
{
    if (ctx_)
    {
        // SSL_CTX_free() decrements the reference count of ctx, and removes
        // the SSL_CTX object pointed to by ctx and frees up the allocated
        // memory if the the reference count has reached 0.
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
    }
}

int32_t TlsCtx::init(const char *cert_file, const char *key_file,
    const char* passwd, const char* ca_path)
{
    // 这里需要处理比较多的异常，不允许放到构造函数里处理

    /* 这个函数会造成较多的内存检测问题
     * Use of uninitialised value of size 8
     * Conditional jump or move depends on uninitialised value(s)
     * https://www.mail-archive.com/openssl-users@openssl.org/msg45215.html
     */

    // 老版本中，需要指定ssl版本，比如SSLv23_method
    // 新版本中，可以直接用TLS_method()来自动协商
    SSL_CTX *ctx = SSL_CTX_new(TLS_method());
    if (!ctx)
    {
        dump_error("new_ssl_ctx:can NOT create ssl content");
        return -1;
    }

    int32_t ok = 0; // 在goto之前声明

    // 调用默认函数校验对方证书的正确性，但不校验域名正确性
    // 要校验域名正确性，需要SSL_set1_host()设置校验的域名
    // SSL_VERIFY_NONE为完全不校验
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
    // 禁用匿名身份验证或者不安全的算法，否则SSL_VERIFY_PEER直接忽略验证
    // If no server certificate is sent, because an anonymous cipher is used, SSL_VERIFY_PEER is ignored.
    SSL_CTX_set_cipher_list(ctx, "HIGH:!aNULL:!kRSA:!PSK:!SRP:!3DES:!MD5:!RC4");

    // 一个ctx可以给多个连接使用，因此一个证书就创建一个ctx就可以了
    // 指定根ca证书路径（通常用于自己签发的证书，否则用系统根证书就行）
    int32_t ret;
    if (ca_path)
    {
        ret = SSL_CTX_load_verify_locations(ctx, ca_path, nullptr);
    }
    else
    {
#ifdef __windows__
        // openssl 3.2 https://stackoverflow.com/questions/9507184/can-openssl-on-windows-use-the-system-certificate-store
        ret = SSL_CTX_load_verify_store(ctx, "org.openssl.winstore://");
#else
        ret = SSL_CTX_set_default_verify_paths(ctx);
#endif
    }
    if (1 != ret)
    {
        PLOG("load verify fail: %s", ca_path ? ca_path : "default");
        dump_error("load verify fail");
        goto FAIL;
    }
    ctx_ = ctx;
    // 没有证书时，默认使用DEFAULT_FILE作名字，仅客户端连接可以没有证书，服务端必须有

    /* 建立ssl时，客户端的证书是在握手阶段由服务器发给客户端的
     * 因此单向认证的客户端使用的SSL_CTX不需要证书
     * 双向认证参考：SSL_CTX_set_verify()这个函数的用法。
     * 设置认证标识后，客户端便会把证书发给服务器，服务器会在
     * SSL_CTX_set_cert_verify_callback中收到回调，校验客户端的证书参数。
     * 一般情况下，如果客户端的域名不是固定的话，这个认证没什么意义的。
     * ssh那种是通过公钥、私钥来认证客户端的，不是证书。
     */
    if (!cert_file || !key_file) return 0;

    // 加载pem格式的ca证书文件，暂不支持ASN1（SSL_FILETYPE_ASN1）格式
    // ASN1只支持一个文件一个证书，pem可以将多个证书放到同一个文件
    if (SSL_CTX_use_certificate_chain_file(ctx, cert_file) <= 0)
    {
        dump_error(cert_file);
        goto FAIL;
    }

    // 加载pem格式私钥
    // 如果key加了密码，则需要处理：SSL_CTX_set_default_passwd_cb
    // PS:ca证书是公开的，但其实也是可以加密码的，这时也要处理才能加载
    // SSL_CTX_check_private_key用到密码，之后一般不用到。但SSL_dump等函数也有用到，所以最好还是存一份
    // 如果不想存，用完后把SSL_CTX_set_default_passwd_cb_userdata设置为空指针，否则会埋坑
    if (passwd)
    {
        // SSL_CTX_set_default_passwd_cb(ctx, ctx_passwd_cb);
        snprintf(passwd_, sizeof(passwd_), "%s", passwd ? passwd : "");
        SSL_CTX_set_default_passwd_cb_userdata(ctx, passwd_);
    }

    // 密钥文件有多种
    // 通用类型，里面以 BEGIN PRIVATE KEY 开头
    // RSA，里面以 BEGIN RSA PRIVATE KEY 开头
    // 现在一般都用的RSA
    ok = SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM);
    // ok = SSL_CTX_use_RSAPrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM);
    if (ok <= 0)
    {
        dump_error("new_ssl_ctx key file");
        goto FAIL;
    }

    // 验证私钥是否和证书匹配
    if (SSL_CTX_check_private_key(ctx) <= 0)
    {
        dump_error("new_ssl_ctx:certificate and private key not match");
        goto FAIL;
    }

    return 0;

FAIL:
    SSL_CTX_free(ctx);
    return -1;
}