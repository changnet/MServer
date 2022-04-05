#include "../../system/static_global.hpp"
#include "ssl_io.hpp"

SSLIO::~SSLIO()
{
    if (_ssl_ctx)
    {
        SSL_free(_ssl_ctx);
        _ssl_ctx = nullptr;
    }
}

SSLIO::SSLIO(uint32_t conn_id, int32_t ssl_id, class Buffer *recv,
             class Buffer *send)
    : IO(conn_id, recv, send)
{
    _ssl_ctx = nullptr;
    _ssl_id  = ssl_id;
}

bool SSLIO::is_ready() const
{
    return 1 == SSL_is_init_finished(_ssl_ctx);
}

IO::IOStatus SSLIO::recv()
{
    assert(Socket::fd_valid(_fd));

    if (!SSL_is_init_finished(_ssl_ctx)) return do_handshake();

    int32_t len = 0;
    while (true)
    {
        Buffer::Transaction &&ts = _recv->any_seserve();
        if (ts._len <= 0) return IOS_BUSY;

        len = SSL_read(_ssl_ctx, ts._ctx, ts._len);
        _recv->commit(ts, len);
        if (EXPECT_FALSE(len <= 0)) break;

        if (len < ts._len) return IOS_OK;
    }

    int32_t ecode = SSL_get_error(_ssl_ctx, len);
    if (SSL_ERROR_WANT_READ == ecode) return IOS_READ;

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
        || (SSL_ERROR_SYSCALL == ecode && !Socket::is_error()))
    {
        return IOS_CLOSE;
    }

    SSLMgr::ssl_error("ssl io recv");
    return IOS_ERROR;
}

IO::IOStatus SSLIO::send()
{
    assert(Socket::fd_valid(_fd));

    if (!SSL_is_init_finished(_ssl_ctx)) return do_handshake();

    int32_t len  = 0;
    size_t bytes = 0;
    bool next    = false;
    while (true)
    {
        const char *data = _send->get_front_used(bytes, next);
        if (0 == bytes) return IOS_OK;

        len = SSL_write(_ssl_ctx, data, (int32_t)bytes);
        if (len <= 0) break;

        _send->remove(len); // 删除已发送数据

        // socket发送缓冲区已满，等下次发送了
        if (len < (int32_t)bytes) return IOS_WRITE;

        // 当前chunk数据已发送完，如果有下一个chunk，则继续发送
        if (!next) return IOS_OK;
    }

    int32_t ecode = SSL_get_error(_ssl_ctx, len);
    if (SSL_ERROR_WANT_WRITE == ecode) return IOS_WRITE;

    // 非主动断开，打印错误日志
    if ((SSL_ERROR_ZERO_RETURN == ecode)
        || (SSL_ERROR_SYSCALL == ecode && !Socket::is_error()))
    {
        return IOS_CLOSE;
    }

    SSLMgr::ssl_error("ssl io send");
    return IOS_ERROR;
}

int32_t SSLIO::init_accept(int32_t fd)
{
    _fd = fd;
    return EV_ACCEPT;
}

int32_t SSLIO::init_connect(int32_t fd)
{
    _fd = fd;
    return EV_CONNECT;
}

IO::IOStatus SSLIO::do_init_accept()
{
    if (init_ssl_ctx() < 0) return IOS_ERROR;

    SSL_set_accept_state(_ssl_ctx);

    return do_handshake();
}

IO::IOStatus SSLIO::do_init_connect()
{
    if (init_ssl_ctx() < 0) return IOS_ERROR;

    SSL_set_connect_state(_ssl_ctx);

    return do_handshake();
}

int32_t SSLIO::init_ssl_ctx()
{
    static class SSLMgr *ctx_mgr = StaticGlobal::ssl_mgr();

    SSL_CTX *base_ctx = ctx_mgr->get_ssl_ctx(_ssl_id);
    if (!base_ctx)
    {
        ELOG("ssl io init ssl ctx no base ctx found: %d", _ssl_id);
        return -1;
    }

    _ssl_ctx = SSL_new(base_ctx);
    if (!_ssl_ctx)
    {
        ELOG("ssl io init ssl SSL_new fail");
        return -1;
    }

    if (!SSL_set_fd(_ssl_ctx, _fd))
    {
        ELOG("ssl io init ssl SSL_set_fd fail");
        return -1;
    }

    return 0;
}

IO::IOStatus SSLIO::do_handshake()
{
    int32_t ecode = SSL_do_handshake(_ssl_ctx);
    if (1 == ecode)
    {
        // SSLMgr::dump_x509(_ssl_ctx);
        // 可能上层在握手期间发送了一些数据，握手成功要检查一下
        // 理论上来讲，SSL可以重新握手，所以这个init_ok可能会触发多次，需要上层逻辑注意
        init_ok();
        return 0 == _send->get_front_used_size() ? IOS_OK : IOS_WRITE;
    }

    /* Caveat: Any TLS/SSL I/O function can lead to either of
     * SSL_ERROR_WANT_READ and SSL_ERROR_WANT_WRITE. In particular, SSL_read()
     * or SSL_peek() may want to write data and SSL_write() may want to read
     * data. This is mainly because TLS/SSL handshakes may occur at any time
     * during the protocol (initiated by either the client or the server);
     * SSL_read(), SSL_peek(), and SSL_write() will handle any pending
     * handshakes.
     */
    ecode = SSL_get_error(_ssl_ctx, ecode);
    if (SSL_ERROR_WANT_READ == ecode) return IOS_READ;
    if (SSL_ERROR_WANT_WRITE == ecode) return IOS_WRITE;

    // error
    SSLMgr::ssl_error(__FUNCTION__, ecode);

    return IOS_ERROR;
}
