#include "../socket_compat.hpp"

#include "io.hpp"
#include "../buffer.hpp"

IO::IO(class Buffer *recv, class Buffer *send)
{
    _fd = -1; // 创建一个io的时候，fd可能还未创建，后面再设置
    _recv = recv;
    _send = send;
}

IO::~IO()
{
    _fd   = -1;
    _recv = nullptr;
    _send = nullptr;
}

// 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
int32_t IO::recv(int32_t &byte)
{
    assert(_fd > 0);

    byte = 0;
    if (!_recv->reserved()) return -1; /* no more memory */

    // epoll当前为LT模式，不用循环读。一般来说缓冲区都分配得比较大，都能读完
    size_t size = _recv->get_space_size();
    int32_t len   = (int32_t)::recv(_fd, _recv->get_space_ctx(), (int32_t)size, 0);
    if (EXPECT_TRUE(len > 0))
    {
        byte = len;
        _recv->add_used_offset(len);
        return 0;
    }

    if (0 == len) return -1; // 对方主动断开

    /* error happen */
    if (errno != EAGAIN && errno != EWOULDBLOCK)
    {
        ELOG("io send:%s", strerror(errno));
        return -1;
    }

    return 1; // 重试
}

// * 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
int32_t IO::send(int32_t &byte)
{
    assert(_fd > 0);

    byte         = 0;
    size_t bytes = _send->get_used_size();
    assert(bytes > 0);

    int32_t len = (int32_t)::send(_fd, _send->get_used_ctx(), (int32_t)bytes, 0);

    if (EXPECT_TRUE(len > 0))
    {
        byte = len;
        _send->remove(len);
        return 0 == _send->get_used_size() ? 0 : 2;
    }

    if (0 == len) return -1; // 对方主动断开

    /* error happen */
    if (errno != EAGAIN && errno != EWOULDBLOCK)
    {
        ELOG("io send:%s", strerror(errno));
        return -1;
    }

    /* need to try again */
    return 2;
}
