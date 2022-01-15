#include "io.hpp"
#include "../socket.hpp"
#include "../../system/static_global.hpp"

#ifdef __windows__
    #include <winsock2.h>
#else
    #include <sys/socket.h>
#endif

IO::IO(uint32_t conn_id, class Buffer *recv, class Buffer *send)
{
    _fd = -1; // 创建一个io的时候，fd可能还未创建，后面再设置
    _conn_id = conn_id;

    _recv = recv;
    _send = send;
}

IO::~IO()
{
    _fd   = -1;
    _recv = nullptr;
    _send = nullptr;
}

IO::IOStatus IO::recv()
{
    assert(Socket::fd_valid(_fd));

    // 用光了所有缓冲区，主线程那边来不及处理
    if (!_recv->reserved()) return IOS_BUSY;

    // epoll当前为LT模式，不用循环读。一般来说缓冲区都分配得比较大，都能读完
    size_t size = _recv->get_space_size();
    int32_t len = (int32_t)::recv(_fd, _recv->get_space_ctx(), (int32_t)size, 0);
    if (EXPECT_TRUE(len > 0))
    {
        _recv->add_used_offset(len);
        return IOS_OK;
    }

    if (0 == len)
    {
        return IOS_CLOSE; // 对方主动断开
    }

    /* error happen */
    if (Socket::is_error())
    {
        ELOG("io recv:%s(%d)", Socket::str_error(), Socket::error_no());
        return IOS_ERROR;
    }

    return IOS_READ; // 重试
}

IO::IOStatus IO::send()
{
    assert(Socket::fd_valid(_fd));

    size_t bytes = _send->get_used_size();
    assert(bytes > 0);

    int32_t len = (int32_t)::send(_fd, _send->get_used_ctx(), (int32_t)bytes, 0);

    if (EXPECT_TRUE(len > 0))
    {
        _send->remove(len);
        return 0 == _send->get_used_size() ? IOS_OK : IOS_WRITE;
    }

    if (0 == len) return IOS_CLOSE; // 对方主动断开

    /* error happen */
    if (Socket::is_error())
    {
        ELOG("io send:%s(%d)", Socket::str_error(), Socket::error_no());
        return IOS_ERROR;
    }

    /* need to try again */
    return IOS_WRITE;
}

int32_t IO::init_accept(int32_t fd)
{
    _fd = fd;
    init_ok();

    return 0;
}

int32_t IO::init_connect(int32_t fd)
{
    _fd = fd;
    init_ok();

    return 0;
}

void IO::init_ok() const
{
    class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();
    network_mgr->io_ok(_conn_id);
}
