#include <openssl/err.h>
#include <openssl/ssl.h>

#include "../../system/static_global.h"
#include "ssl_io.h"

#define X_SSL(x)     static_cast<SSL *>(x)
#define X_SSL_CTX(x) static_cast<SSL_CTX *>(x)

// SSL的错误码是按队列存放的，一次错误可以产生多个错误码
// 因此出错时，需要循环用ERR_get_error来清空错误码或者调用ERR_clear_error
#define SSL_ERROR(x)                                      \
    do                                                    \
    {                                                     \
        ERROR(x " errno(%d:%s)", errno, strerror(errno)); \
        int32_t eno = 0;                                  \
        while (0 != (eno = ERR_get_error()))              \
        {                                                 \
            ERROR("    %s", ERR_error_string(eno, NULL)); \
        }                                                 \
    } while (0)

SSLIO::~SSLIO()
{
    if (_ssl_ctx)
    {
        SSL_free(X_SSL(_ssl_ctx));
        _ssl_ctx = NULL;
    }
}

SSLIO::SSLIO(const char *cert, class Buffer *recv, class Buffer *send)
    : IO(recv, send)
{
    _handshake = false;
    _ssl_ctx   = NULL;

    snprintf(_cert, sizeof(_cert), "%s", cert ? cert : "");
}

/* 接收数据
 * * 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
 */
int32_t SSLIO::recv(int32_t &byte)
{
    ASSERT(_fd > 0, "io recv fd invalid");

    byte = 0;
    if (!_handshake) return do_handshake();

    if (!_recv->reserved()) return -1; /* no more memory */

    // ERR_clear_error
    uint32_t size = _recv->get_space_size();
    int32_t len   = SSL_read(X_SSL(_ssl_ctx), _recv->get_space_ctx(), size);
    if (EXPECT_TRUE(len > 0))
    {
        byte = len;
        _recv->add_used_offset(len);
        return 0;
    }

    int32_t ecode = SSL_get_error(X_SSL(_ssl_ctx), len);
    if (SSL_ERROR_WANT_READ == ecode) return 1;

    /* https://www.openssl.org/docs/manmaster/man3/SSL_read.html
     * SSL连接关闭时，要先关闭SSL协议，再关闭socket。当一个连接直接关闭时，SSL并不能明确
     * 区分开来。SSL_ERROR_ZERO_RETURN仅仅是表示SSL协议层关闭，连接并没有关闭(你可以把一
     * 个SSL连接转化为一个非SSL连接，参考SSL_shutdown)。正常关闭下，SSL_read先收到一个
     * SSL_ERROR_ZERO_RETURN转换为普通连接，然后read再收到一个0。如果直接关闭，则SSL返回
     * 0，SSL_get_error检测到syscall错误(即read返回0)，这时errno为0,SSL_get_error并返
     * 回0。
     */

    // 非主动断开，打印错误日志
    // 在实际测试中，chrome会直接断开链接，而firefox则会关闭SSL */
    if ((SSL_ERROR_ZERO_RETURN == ecode)
        || (SSL_ERROR_SYSCALL == ecode && 0 == errno))
    {
        return -1;
    }

    SSL_ERROR("ssl io recv");
    return -1;
}

/* 发送数据
 * * 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
 */
int32_t SSLIO::send(int32_t &byte)
{
    ASSERT(_fd > 0, "io send fd invalid");

    byte = 0;
    if (!_handshake) return do_handshake();

    size_t bytes = _send->get_used_size();
    ASSERT(bytes > 0, "io send without data");

    int32_t len = SSL_write(X_SSL(_ssl_ctx), _send->get_used_ctx(), bytes);
    if (EXPECT_TRUE(len > 0))
    {
        byte = len;
        _send->remove(len);
        return 0 == _send->get_used_size() ? 0 : 2;
    }

    int32_t ecode = SSL_get_error(X_SSL(_ssl_ctx), len);
    if (SSL_ERROR_WANT_WRITE == ecode) return 2;

    // 非主动断开，打印错误日志
    if ((SSL_ERROR_ZERO_RETURN == ecode)
        || (SSL_ERROR_SYSCALL == ecode && 0 == errno))
    {
        return -1;
    }

    SSL_ERROR("ssl io send");
    return -1;
}

/* 准备接受状态
 */
int32_t SSLIO::init_accept(int32_t fd)
{
    if (init_ssl_ctx(fd) < 0) return -1;

    _fd = fd;
    SSL_set_accept_state(X_SSL(_ssl_ctx));

    return do_handshake();
}

/* 准备连接状态
 */
int32_t SSLIO::init_connect(int32_t fd)
{
    if (init_ssl_ctx(fd) < 0) return -1;

    _fd = fd;
    SSL_set_connect_state(X_SSL(_ssl_ctx));

    return do_handshake();
}

int32_t SSLIO::init_ssl_ctx(int32_t fd)
{
    static class SSLMgr *ctx_mgr = StaticGlobal::ssl_mgr();

    void *base_ctx = ctx_mgr->get_ssl_ctx(_cert);
    if (!base_ctx)
    {
        ERROR("ssl io init ssl ctx no base ctx found");
        return -1;
    }

    _ssl_ctx = SSL_new(X_SSL_CTX(base_ctx));
    if (!_ssl_ctx)
    {
        ERROR("ssl io init ssl SSL_new fail");
        return -1;
    }

    if (!SSL_set_fd(X_SSL(_ssl_ctx), fd))
    {
        ERROR("ssl io init ssl SSL_set_fd fail");
        return -1;
    }

    return 0;
}

// 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
int32_t SSLIO::do_handshake()
{
    int32_t ecode = SSL_do_handshake(X_SSL(_ssl_ctx));
    if (1 == ecode)
    {
        _handshake = true;
        // 可能上层在握手期间发送了一些数据，握手成功要检查一下
        return 0 == _send->get_used_size() ? 0 : 2;
    }

    /* Caveat: Any TLS/SSL I/O function can lead to either of
     * SSL_ERROR_WANT_READ and SSL_ERROR_WANT_WRITE. In particular, SSL_read()
     * or SSL_peek() may want to write data and SSL_write() may want to read
     * data. This is mainly because TLS/SSL handshakes may occur at any time
     * during the protocol (initiated by either the client or the server);
     * SSL_read(), SSL_peek(), and SSL_write() will handle any pending
     * handshakes.
     */
    ecode = SSL_get_error(X_SSL(_ssl_ctx), ecode);
    if (SSL_ERROR_WANT_READ == ecode) return 1;
    if (SSL_ERROR_WANT_WRITE == ecode) return 2;

    // error
    SSL_ERROR("ssl io do handshake:");

    return -1;
}
