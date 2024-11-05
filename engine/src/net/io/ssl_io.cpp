#include "ssl_io.hpp"

#include "system/static_global.hpp"
#include "net/net_compat.hpp"
#include "net/io/tls_ctx.hpp"

SSLIO::~SSLIO()
{
    if (_ssl)
    {
        SSL_free(_ssl);
        _ssl = nullptr;
    }
}

SSLIO::SSLIO(int32_t conn_id, TlsCtx *tls_ctx)
    : IO(conn_id)
{
    _ssl = nullptr;
    _tls_ctx = tls_ctx;
}

int32_t SSLIO::recv(EVIO *w)
{
    if (!SSL_is_init_finished(_ssl)) return do_handshake();

    int32_t len = 0;
    while (true)
    {
        Buffer::Transaction &&ts = _recv.any_seserve(true);
        if (ts._len <= 0) return EV_BUSY;

        len = SSL_read(_ssl, ts._ctx, ts._len);
        _recv.commit(ts, len);
        if (EXPECT_FALSE(len <= 0)) break;

        if (len < ts._len) return EV_NONE;
    }

    int32_t ecode = SSL_get_error(_ssl, len);
    if (SSL_ERROR_WANT_READ == ecode) return EV_READ;

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
        || (SSL_ERROR_SYSCALL == ecode
            && !netcompat::iserror(netcompat::errorno())))
    {
        return EV_CLOSE;
    }

    w->_errno = netcompat::errorno();
    TlsCtx::dump_error("ssl io recv");
    return EV_ERROR;
}

int32_t SSLIO::send(EVIO *w)
{
    if (!SSL_is_init_finished(_ssl)) return do_handshake();

    int32_t len  = 0;
    size_t bytes = 0;
    bool next    = false;
    while (true)
    {
        const char *data = _send.get_front_used(bytes, next);
        if (0 == bytes) return EV_NONE;

        len = SSL_write(_ssl, data, (int32_t)bytes);
        if (len <= 0) break;

        _send.remove(len); // 删除已发送数据

        // socket发送缓冲区已满，等下次发送了
        if (len < (int32_t)bytes) return EV_WRITE;

        // 当前chunk数据已发送完，如果有下一个chunk，则继续发送
        if (!next) return EV_NONE;
    }

    int32_t ecode = SSL_get_error(_ssl, len);
    if (SSL_ERROR_WANT_WRITE == ecode) return EV_WRITE;

    // 非主动断开，打印错误日志
    if ((SSL_ERROR_ZERO_RETURN == ecode)
        || (SSL_ERROR_SYSCALL == ecode
            && !netcompat::iserror(netcompat::errorno())))
    {
        return EV_CLOSE;
    }

    w->_errno = netcompat::errorno();
    TlsCtx::dump_error("ssl io send");
    return EV_ERROR;
}

int32_t SSLIO::do_init_accept(int32_t fd)
{
    if (init_ssl_ctx(fd) < 0) return EV_ERROR;

    SSL_set_accept_state(_ssl);

    return do_handshake();
}

int32_t SSLIO::do_init_connect(int32_t fd)
{
    if (init_ssl_ctx(fd) < 0) return EV_ERROR;

    SSL_set_connect_state(_ssl);

    return do_handshake();
}

int32_t SSLIO::init_ssl_ctx(int32_t fd)
{
    _ssl = SSL_new(_tls_ctx->get());
    if (!_ssl)
    {
        ELOG("ssl io init ssl SSL_new fail");
        return -1;
    }

    if (!SSL_set_fd(_ssl, fd))
    {
        ELOG("ssl io init ssl SSL_set_fd fail");
        return -1;
    }

    return 0;
}

int32_t SSLIO::do_handshake()
{
    int32_t ecode = SSL_do_handshake(_ssl);
    if (1 == ecode)
    {
        // SSLMgr::dump_x509(_ssl);
        // 可能上层在握手期间发送了一些数据，握手成功要检查一下
        // 理论上来讲，SSL可以重新握手，所以这个init_ok可能会触发多次，需要上层逻辑注意
        return SSL_is_server(_ssl) ? EV_INIT_ACPT : EV_INIT_CONN;
    }

    /* Caveat: Any TLS/SSL I/O function can lead to either of
     * SSL_ERROR_WANT_READ and SSL_ERROR_WANT_WRITE. In particular, SSL_read()
     * or SSL_peek() may want to write data and SSL_write() may want to read
     * data. This is mainly because TLS/SSL handshakes may occur at any time
     * during the protocol (initiated by either the client or the server);
     * SSL_read(), SSL_peek(), and SSL_write() will handle any pending
     * handshakes.
     */
    ecode = SSL_get_error(_ssl, ecode);

    // 握手无法一次完成，必须返回事件让socket执行继续do_handshake
    if (SSL_ERROR_WANT_READ == ecode) return EV_READ;
    if (SSL_ERROR_WANT_WRITE == ecode) return EV_WRITE;

    // error
    TlsCtx::dump_error(__FUNCTION__, ecode);

    return EV_ERROR;
}
