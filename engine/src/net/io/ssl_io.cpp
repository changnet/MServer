
#include "ssl_io.hpp"

#include "system/static_global.hpp"
#include "net/net_compat.hpp"
#include "net/io/tls_ctx.hpp"
#include "ev/ev_watcher.hpp"

SSLIO::~SSLIO()
{
    if (ssl_)
    {
        SSL_free(ssl_);
        ssl_ = nullptr;
    }
}

SSLIO::SSLIO(TlsCtx *tls_ctx)
{
    ssl_ = SSL_new(tls_ctx->get());
}

int32_t SSLIO::recv(EVIO *w)
{
    if (!SSL_is_init_finished(ssl_)) return handshake(w);

    int32_t len = 0;
    // 第一次读取，尝试利用 buffer 尾部剩余空间
    // 初始空间有8k，对于游戏来说应该是足够的，大部分情况不需要后续分配
    Buffer::Chunk *tail = recv_.get_back();

    int32_t space = 0;
    size_t alloc_size = Buffer::CHUNK_SIZE;
    char *buffer_ptr = tail->write_ptr(space);
    if (space > 0)
    {
        len = SSL_read(ssl_, buffer_ptr, space);
        if (len > 0)
        {
            recv_.add_used(tail, len);
            if (len < space) return EV_NONE;
        }
        else
        {
            goto SSL_RECV_ERROR;
        }
    }

    while (true)
    {
        if (recv_.is_overflow()) return EV_READ;

        Buffer::Chunk *chk = recv_.allocate_chunk(alloc_size);

        len = SSL_read(ssl_, chk->data(), chk->capacity_);
        if (len > 0)
        {
            recv_.append_chunk(chk, len);
            if (len < chk->capacity_) return EV_NONE;

            alloc_size *= 2; // 指数增长
        }
        else
        {
            recv_.deallocate_chunk(chk);

            goto SSL_RECV_ERROR;
        }
    }

    assert(false); // never reach here
SSL_RECV_ERROR:
    int32_t ecode = SSL_get_error(ssl_, len);
    if (SSL_ERROR_WANT_READ == ecode) return EV_READ;
    if (SSL_ERROR_WANT_WRITE == ecode) return EV_WRITE;

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
        w->mask_.fetch_or(EVIO::M_REMOTE_CLOSE);
        return EV_CLOSE;
    }

    w->errno_ = netcompat::errorno();
    TlsCtx::dump_error("ssl io recv");
    return EV_ERROR;
}

int32_t SSLIO::send(EVIO *w)
{
    if (!SSL_is_init_finished(ssl_)) return handshake(w);

    int32_t len  = 0;
    size_t bytes = 0;
    while (true)
    {
        const char *data = send_.get_front_data(bytes);
        if (0 == bytes) return EV_NONE;

        len = SSL_write(ssl_, data, (int32_t)bytes);
        if (len > 0)
        {
            send_.remove(len);
            if (len < (int32_t)bytes) return EV_WRITE;
        }
        else
        {
            int32_t ecode = SSL_get_error(ssl_, len);
            if (SSL_ERROR_WANT_WRITE == ecode) return EV_WRITE;
            if (SSL_ERROR_WANT_READ == ecode) return EV_READ;

            if ((SSL_ERROR_ZERO_RETURN == ecode)
                || (SSL_ERROR_SYSCALL == ecode
                    && !netcompat::iserror(netcompat::errorno())))
            {
                w->mask_.fetch_or(EVIO::M_REMOTE_CLOSE);
                return EV_CLOSE;
            }

            w->errno_ = netcompat::errorno();
            TlsCtx::dump_error("ssl io send");
            return EV_ERROR;
        }
    }

    return EV_WRITE;
}

int32_t SSLIO::do_init_accept(EVIO *w)
{
    if (init_ssl_ctx(w->fd_) < 0) return EV_ERROR;

    SSL_set_accept_state(ssl_);

    return handshake(w);
}

int32_t SSLIO::do_init_connect(EVIO *w)
{
    if (init_ssl_ctx(w->fd_) < 0) return EV_ERROR;

    SSL_set_connect_state(ssl_);

    return handshake(w);
}

[[maybe_unused]] static void info_callback(const SSL *ssl, int where, int ret)
{
    const char *str;
    int32_t fd = SSL_get_fd(ssl);
    int32_t w  = where & ~SSL_ST_MASK;
    if (w & SSL_ST_CONNECT)
    {
        str = "SSL_connect";
    }
    else if (w & SSL_ST_ACCEPT)
    {
        str = "SSL_accept";
    }
    else
    {
        str = "undefined";
    }
    if (where & SSL_CB_LOOP)
    {
        PLOG("Loop: fd = %d, %s:%s", fd, str, SSL_state_string_long(ssl));
    }
    else if (where & SSL_CB_ALERT)
    {
        str = (where & SSL_CB_READ) ? "read" : "write";
        PLOG("Alert: fd = %d %s:%s:%s", fd, str, SSL_alert_type_string(ret),
             SSL_alert_desc_string(ret));
    }
    else if (where & SSL_CB_EXIT)
    {
        if (0 == ret)
        {
            PLOG("Exit: fd = %d %s:failed in %s", fd, str,
                 SSL_state_string_long(ssl));
        }
        else if (ret < 0)
        {
            PLOG("Exit: fd = %d %s:error in %s", fd, str,
                 SSL_state_string_long(ssl));
        }
    }
    else if (where & SSL_CB_HANDSHAKE_START)
    {
        PLOG("Handshake: fd = %d started", fd);
    }
    else if (where & SSL_CB_HANDSHAKE_DONE)
    {
        PLOG("Handshake: fd = %d done", fd);
    }
}

[[maybe_unused]] static void tlsext_callback(SSL *ssl, int32_t client_server,
                                             int32_t type,
                                             const unsigned char *data,
                                             size_t len, void *arg)
{
    // 可用第三方工具查看连接的sni和alpn
    // openssl s_client -connect ws.postman-echo.com:443 -servername
    // ws.postman-echo.com -tlsextdebug curl -v -X GET
    // https://postman-echo.com/get
    if (type == TLSEXT_TYPE_server_name)
    {
        if (len >= 5)
        {
            uint16_t list_len = (data[0] << 8) | data[1];
            if ((size_t)list_len + 2 <= len)
            {
                uint8_t name_type = data[2];
                if (name_type == 0)
                { // host_name
                    uint16_t name_len = (data[3] << 8) | data[4];
                    if ((size_t)name_len + 5 <= len)
                    {
                        PLOG("ssl SNI found, len = %u, name = %.*s", name_len,
                             name_len, data + 5);
                    }
                }
            }
        }
    }
    else if (type == TLSEXT_TYPE_application_layer_protocol_negotiation)
    {
        PLOG("ssl ALPN found, len = %zu", len);
        if (len >= 2)
        {
            const unsigned char *p = data;
            uint32_t alpn_len      = (p[0] << 8) | p[1];
            p += 2;
            while (alpn_len > 0 && p < data + len)
            {
                uint32_t proto_len = *p++;
                if (proto_len > 0 && p + proto_len <= data + len)
                {
                    PLOG("    protocol: %.*s", proto_len, p);
                    p += proto_len;
                    alpn_len -= 1 + proto_len;
                }
                else
                {
                    break;
                }
            }
        }
    }
}

int32_t SSLIO::init_ssl_ctx(int32_t fd)
{
    if (!ssl_)
    {
        ELOG("ssl io init ssl SSL_new fail");
        return -1;
    }

    if (!SSL_set_fd(ssl_, fd))
    {
        ELOG("ssl io init ssl SSL_set_fd fail");
        return -1;
    }

#ifdef SSL_DBG
    SSL_set_info_callback(ssl_, info_callback);
    SSL_set_tlsext_debug_callback(ssl_, tlsext_callback);
#endif

    return 0;
}

int32_t SSLIO::handshake(EVIO *w)
{
    int32_t ecode = SSL_do_handshake(ssl_);
#ifdef SSL_DBG
    PLOG("ssl handshake fd = %d, code = %d", w->fd_, ecode);
#endif
    if (1 == ecode)
    {
#ifdef SSL_DBG
        uint32_t alpn_len;
        const unsigned char *alpn;
        SSL_get0_alpn_selected(ssl_, &alpn, &alpn_len);
        PLOG("ssl handshake ready fd = %d, SNI = %s, ALPN = %.*s", w->fd_,
             SSL_get_servername(ssl_, TLSEXT_NAMETYPE_host_name), alpn_len, alpn);
#endif
        // SSLMgr::dump_x509(ssl_);
        // 可能上层在握手期间发送了一些数据，握手成功要检查一下
        // 理论上来讲，SSL可以重新握手，所以这个init_ok可能会触发多次，需要上层逻辑注意
        return SSL_is_server(ssl_) ? EV_INIT_ACPT : EV_INIT_CONN;
    }

    /* Caveat: Any TLS/SSL I/O function can lead to either of
     * SSL_ERROR_WANT_READ and SSL_ERROR_WANT_WRITE. In particular, SSL_read()
     * or SSL_peek() may want to write data and SSL_write() may want to read
     * data. This is mainly because TLS/SSL handshakes may occur at any time
     * during the protocol (initiated by either the client or the server);
     * SSL_read(), SSL_peek(), and SSL_write() will handle any pending
     * handshakes.
     */
    ecode = SSL_get_error(ssl_, ecode);
#ifdef SSL_DBG
    PLOG("ssl handshake get error fd = %d, code = %d", w->fd_, ecode);
#endif

    // 握手无法一次完成，必须返回事件让socket执行继续do_handshake
    if (SSL_ERROR_WANT_READ == ecode) return EV_READ;
    if (SSL_ERROR_WANT_WRITE == ecode) return EV_WRITE;

    // error
    w->errno_ = netcompat::errorno();
    TlsCtx::dump_error(__FUNCTION__, ecode);

    return EV_ERROR;
}

int32_t SSLIO::set_ssl_alpn(int32_t alpn)
{
    if (!ssl_) return -1;

    size_t alpn_size;
    const unsigned char *alpn_protos;
    switch (alpn)
    {
    case 1:
        alpn_size   = 9;
        alpn_protos = (const unsigned char *)"\x08http/1.0";
        break;
    case 2:
        alpn_size   = 9;
        alpn_protos = (const unsigned char *)"\x08http/1.1";
        break;
    case 3:
        alpn_size   = 3;
        alpn_protos = (const unsigned char *)"\x02h2"; // http/2
        break;
    case 4:
        alpn_size = 12;
        alpn_protos =
            (const unsigned char *)"\x02h2\x08http/1.1"; // http/2 + http/1.1
        break;
    // 其他的，还有grpc等等，暂时不管
    default: return -3;
    }
#ifdef SSL_DBG
    PLOG("ssl fd = %d set alpn", SSL_get_fd(ssl_));
    size_t index = 0;
    while (index < alpn_size)
    {
        uint8_t len = (uint8_t) * (alpn_protos + index);
        index++;
        PLOG("    protocol: %.*s", (int32_t)len, alpn_protos + index);

        index += len;
    }
#endif
    return SSL_set_alpn_protos(ssl_, alpn_protos, (uint32_t)alpn_size);
}

int32_t SSLIO::set_ssl_sni(const char *sni)
{
    if (!ssl_) return -1;

#ifdef SSL_DBG
    PLOG("ssl fd = %d set sni = %s", SSL_get_fd(ssl_), sni);
#endif

    // sni用于指定当前连接是连到哪个服务。
    // 在虚拟主机（Virtual Hosting）中，可以一个ip部署多个服务。
    // postman-echo.com不指定sni时，ssl握手会失败。对方直接关闭连接，没有其他错误信息
    return (int32_t)SSL_set_tlsext_host_name(ssl_, sni);
}
int32_t SSLIO::set_ssl_cert_host(const char *host)
{
    if (!ssl_) return -1;

#ifdef SSL_DBG
    PLOG("ssl fd = %d set certificate host = %s", SSL_get_fd(ssl_), host);
#endif

    // 只有指定了证书域名，ssl握手时才会验证域名和证书是否匹配
    // 证书域名和sni一般情况下是一致的，但并不一定。
    // 比如使用了负载均衡时，sni为负载均衡的域名lb.example.com，但证书域名可能是另一个example.com
    return SSL_set1_host(ssl_, host);
}

int32_t SSLIO::set_ssl_verify_mode(int32_t mode)
{
    if (!ssl_) return -1;

    // @param mode 值必须对应 SSL_VERIFY_PEER 等宏定义
    SSL_set_verify(ssl_, mode, nullptr);

    return 0;
}
