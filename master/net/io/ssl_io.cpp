#include <openssl/ssl.h>
#include <openssl/err.h>

#include "ssl_io.h"
#include "ssl_mgr.h"

#define X_SSL(x) static_cast<SSL *>( x )
#define X_SSL_CTX(x) static_cast<SSL_CTX *>( x )

ssl_io::~ssl_io()
{
}

ssl_io::ssl_io( int32 ctx_idx,class buffer *recv,class buffer *send )
    : io( recv,send )
{
    _handshake = false;
    _ssl_ctx = NULL;
    _ctx_idx = ctx_idx;
}

/* 接收数据
 * 返回：< 0 错误(包括对方主动断开)，0 需要重试，> 0 成功读取的字节数
 */
int32 ssl_io::recv()
{
    assert( "io recv fd invalid",_fd > 0 );

    return 0;
}

/* 发送数据
 * 返回：< 0 错误(包括对方主动断开)，0 成功，> 0 仍需要发送的字节数
 */
int32 ssl_io::send()
{
    assert( "io send fd invalid",_fd > 0 );

    return 0;
}

/* 准备接受状态
 */
int32 ssl_io::init_accept( int32 fd )
{
    if ( init_ssl_ctx( fd ) < 0 ) return -1;

    _fd = fd;
    SSL_set_accept_state( X_SSL( _ssl_ctx ) );
    return 0;
}

/* 准备连接状态
 */
int32 ssl_io::init_connect( int32 fd )
{
    _fd = fd;
    return 0;
}

int32 ssl_io::init_ssl_ctx( int32 fd )
{
    static class ssl_mgr *ctx_mgr = ssl_mgr::instance();

    void *base_ctx = ctx_mgr->get_ssl_ctx( _ctx_idx );
    if ( !base_ctx )
    {
        ERROR( "ssl io init ssl ctx no base ctx found" );
        return -1;
    }

    _ssl_ctx = SSL_new( X_SSL_CTX( base_ctx ) );
    if ( !_ssl_ctx )
    {
        ERROR( "ssl io init ssl SSL_new fail" );
        return -1;
    }

    if ( !SSL_set_fd( X_SSL( _ssl_ctx ),fd ) )
    {
        ERROR( "ssl io init ssl SSL_set_fd fail" );
        return -1;
    }

    return 0;
}

// 返回: < 0 错误，0  需要重试，> 0 成功
int32 ssl_io::do_handshake()
{
    int32 ecode = SSL_do_handshake( X_SSL( _ssl_ctx ) );
    if ( 1 == ecode )
    {
        _handshake = true;
        return 1;
    }

    /* Caveat: Any TLS/SSL I/O function can lead to either of 
     * SSL_ERROR_WANT_READ and SSL_ERROR_WANT_WRITE. In particular, SSL_read() 
     * or SSL_peek() may want to write data and SSL_write() may want to read 
     * data. This is mainly because TLS/SSL handshakes may occur at any time 
     * during the protocol (initiated by either the client or the server); 
     * SSL_read(), SSL_peek(), and SSL_write() will handle any pending 
     * handshakes.
     */
    ecode = SSL_get_error( X_SSL( _ssl_ctx ),ecode );
    if ( SSL_ERROR_WANT_WRITE == ecode || SSL_ERROR_WANT_READ == ecode )
    {
        return 0;
    }

    // error
    ERROR( "ssl io do handshake "
        "error.system:%s,ssl:%s",strerror(errno),ERR_error_string(ecode,NULL) );

    return -1;
}
