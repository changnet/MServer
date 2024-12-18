#include "io.hpp"
#include "lpp/lcpp.hpp"
#include "net/net_compat.hpp"
#include "system/static_global.hpp"

#ifdef __windows__
    #include <winsock2.h>
#else
    #include <sys/socket.h>
#endif

IO::IO(int32_t conn_id)
{
    conn_id_ = conn_id;
}

IO::~IO()
{
}

int32_t IO::recv(EVIO *w)
{
    int32_t fd = w->fd_;
    assert(fd != netcompat::INVALID);

    int32_t len = 0;
    while (true)
    {
        Buffer::Transaction &&ts = recv_.any_seserve(true);
        if (0 == ts.len_) return EV_BUSY;

        len = (int32_t)::recv(fd, ts.ctx_, ts.len_, 0);

        recv_.commit(ts, len);
        if (unlikely(len <= 0)) break;

        // 如果没读满缓冲区，则所有数据已读出来
        if (len < ts.len_) return EV_NONE;
    }

    if (0 == len)
    {
        return EV_CLOSE; // 对方主动断开
    }

    /* error happen */
    int32_t e = netcompat::errorno();
    if (netcompat::iserror(e))
    {
        w->errno_ = e;
        ELOG("io recv fd = %d:%s(%d)", fd, netcompat::strerror(e), e);
        return EV_ERROR;
    }

    return EV_READ; // 重试
}

int32_t IO::send(EVIO *w)
{
    int32_t fd = w->fd_;
    assert(fd != netcompat::INVALID);

    int32_t len  = 0;
    size_t bytes = 0;
    bool next    = false;
    while (true)
    {
        const char *data = send_.get_front_used(bytes, next);
        if (0 == bytes) return EV_NONE;

        len = (int32_t)::send(fd, data, (int32_t)bytes, 0);
        if (len <= 0) break;

        send_.remove(len); // 删除已发送数据

        // socket发送缓冲区已满，等下次发送了
        if (len < (int32_t)bytes) return EV_WRITE;

        // 当前chunk数据已发送完，如果有下一个chunk，则继续发送
        if (!next) return EV_NONE;
    }

    if (0 == len) return EV_CLOSE; // 对方主动断开

    /* error happen */
    int32_t e = netcompat::errorno();
    if (netcompat::iserror(e))
    {
        w->errno_ = e;
        ELOG("io send fd = %d:%s(%d)", fd, netcompat::strerror(e), e);
        return EV_ERROR;
    }

    /* need to try again */
    return EV_WRITE;
}

void IO::init_ready() const
{
    try
    {
        lcpp::call(StaticGlobal::L, "conn_io_ready", conn_id_);
    }
    catch (const std::runtime_error& e)
    {
        ELOG("%s", e.what());
    }
}
