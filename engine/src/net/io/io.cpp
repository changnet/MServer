#include "io.hpp"
#include "net/net_compat.hpp"
#include "system/static_global.hpp"

#ifdef __windows__
    #include <winsock2.h>
#else
    #include <sys/socket.h>
#endif

IO::IO(int32_t conn_id)
{
    _conn_id = conn_id;
}

IO::~IO()
{
}

int32_t IO::recv(EVIO *w)
{
    int32_t fd = w->_fd;
    assert(fd != netcompat::INVALID);

    int32_t len = 0;
    while (true)
    {
        Buffer::Transaction &&ts = _recv.any_seserve(true);
        if (0 == ts._len) return EV_BUSY;

        len = (int32_t)::recv(fd, ts._ctx, ts._len, 0);

        _recv.commit(ts, len);
        if (unlikely(len <= 0)) break;

        // 如果没读满缓冲区，则所有数据已读出来
        if (len < ts._len) return EV_NONE;
    }

    if (0 == len)
    {
        return EV_CLOSE; // 对方主动断开
    }

    /* error happen */
    int32_t e = netcompat::errorno();
    if (netcompat::iserror(e))
    {
        w->_errno = e;
        ELOG("io recv fd = %d:%s(%d)", fd, netcompat::strerror(e), e);
        return EV_ERROR;
    }

    return EV_READ; // 重试
}

int32_t IO::send(EVIO *w)
{
    int32_t fd = w->_fd;
    assert(fd != netcompat::INVALID);

    int32_t len  = 0;
    size_t bytes = 0;
    bool next    = false;
    while (true)
    {
        const char *data = _send.get_front_used(bytes, next);
        if (0 == bytes) return EV_NONE;

        len = (int32_t)::send(fd, data, (int32_t)bytes, 0);
        if (len <= 0) break;

        _send.remove(len); // 删除已发送数据

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
        w->_errno = e;
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
        lcpp::call(StaticGlobal::L, "conn_io_ready", _conn_id);
    }
    catch (const std::runtime_error& e)
    {
        ELOG("%s", e.what());
    }
}
