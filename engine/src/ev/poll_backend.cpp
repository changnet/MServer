#include "net/socketpair.hpp"

#include "poll_backend.hpp"

#ifdef __windows__
    const char *__BACKEND__ = "WSAPoll";
#else
    const char *__BACKEND__ = "poll";
#endif

PollBackend::PollBackend() : _fd_index(HUGE_FD + 1, -1)
{
    _wake_fd[0] = _wake_fd[1] = -1;
    _poll_fd.reserve(1024);
}

PollBackend::~PollBackend()
{
}

void PollBackend::after_stop()
{
    if (_wake_fd[0] != netcompat::INVALID) netcompat::close(_wake_fd[0]);
    if (_wake_fd[1] != netcompat::INVALID) netcompat::close(_wake_fd[1]);
}

bool PollBackend::before_start()
{
    /**
     * WSAPoll有个bug，connect失败不会触发事件，直到win10 2004才修复
     * https://stackoverflow.com/questions/21653003/is-this-wsapoll-bug-for-non-blocking-sockets-fixed
     * https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsapoll
     * As of Windows 10 version 2004, when a TCP socket fails to connect,
     * (POLLHUP \| POLLERR \| POLLWRNORM) is indicated
     */

    // https://docs.microsoft.com/zh-cn/windows-hardware/drivers/ddi/wdm/nf-wdm-rtlisntddiversionavailable
    // NTDDI_WIN10_VB	Windows 10 2004

#ifdef __windows__
    #if !(NTDDI_WIN10_VB && NTDDI_VERSION >= NTDDI_WIN10_VB)
        #pragma message("windows version < WIN 10 2004, WSAPoll has fatal bug")

        // 不太清楚VS版本和运行版本之间的关系，全部按编译时打印警告
        // 在WIN10 19H1上编译的程序即使在WIN10 21H1上运行，GetVersion()函数还是返回19H1
        PLOG("windows version < WIN 10 2004, WSAPoll has fatal bug");
    #endif
#endif

    if (socketpair(AF_LOCAL, SOCK_STREAM, 0, _wake_fd) < 0)
    {
        int32_t e = netcompat::errorno();
        ELOG("poll init socketpair fail(%d): %s", e, netcompat::strerror(e));

        return false;
    }

    if (modify_fd(_wake_fd[1], FD_OP_ADD, EV_READ) < 0)
    {
        return false;
    }

    return true;
}

void PollBackend::wake()
{
    static const int8_t v = 1;
    ::send(_wake_fd[0], (const char *)&v, sizeof(v), 0);
}

void PollBackend::do_wait_event(int32_t ev_count)
{
    if (ev_count <= 0) return;

    for (auto &pf : _poll_fd)
    {
        int32_t revents = pf.revents;
        if (!revents) continue;

        auto fd = pf.fd;
        if (EXPECT_FALSE(revents & POLLNVAL))
        {
            ELOG("poll invalid fd: " FMT64d, (int64_t)fd);
            assert(false);
        }

        if (fd == _wake_fd[1])
        {
            // TODO 一般都能一次读出所有数据，即使有未读出，也只是多循环一次
            static char buffer[512];
            int32_t byte = (int32_t)::recv(fd, buffer, sizeof(buffer), 0);

            // 这个连接在程序运行时应该永远不会关闭
            assert(byte > 0);
        }
        else
        {
            int32_t events = 0;
            if (revents & POLLOUT) events |= EV_WRITE;
            if (revents & POLLIN) events |= EV_READ;
            if (revents & (POLLERR | POLLHUP)) events |= EV_CLOSE;

            EVIO *w = _fd_mgr.get(static_cast<int32_t>(fd));
            assert(w);
            do_watcher_wait_event(w, events);
        }

        // poll返回值表示数组中有N个fd有事件，但只能遍历来查找哪个有事件
        ev_count--;
        if (0 == ev_count) break;
    }
}

int32_t PollBackend::wait(int32_t timeout)
{
#ifdef __windows__
    int32_t ev_count = WSAPoll(_poll_fd.data(), (ULONG)_poll_fd.size(), timeout);
#else
    int32_t ev_count = poll(_poll_fd.data(), _poll_fd.size(), timeout);
#endif
    if (EXPECT_FALSE(ev_count < 0))
    {
        switch (errno)
        {
        case EINTR: return 0;
        case ENOMEM:
            ELOG("poll ENOMEM");
            return -1;
        default:
        {
            int32_t e = netcompat::errorno();
            ELOG("poll fatal(%d): %s", e, netcompat::strerror(e));
            assert(false);
            return -1;
        }
        }
    }

    return ev_count;
}

int32_t PollBackend::del_fd_index(int32_t fd)
{
    int32_t index = -1;
    uint32_t ufd = ((uint32_t)fd);
    if (ufd < HUGE_FD)
    {
        if (fd < _fd_index.size())
        {
            index          = _fd_index[ufd];
            _fd_index[ufd] = -1;
        }
    }
    else
    {
        auto found = _fd_index_huge.find(fd);
        if (found != _fd_index_huge.end()) index = found->second;

        _fd_index_huge.erase(fd);
    }

    assert(index >= 0);
    return index;
}

int32_t PollBackend::get_fd_index(int32_t fd)
{
    uint32_t ufd = ((uint32_t)fd);
    if (ufd < HUGE_FD)
    {
        return fd >= _fd_index.size() ? -1 : _fd_index[ufd];
    }

    auto found = _fd_index_huge.find(fd);
    return found == _fd_index_huge.end() ? -1 : found->second;
}

int32_t PollBackend::add_fd_index(int32_t fd)
{
    int32_t index = (int32_t)_poll_fd.size();

    uint32_t ufd = ((uint32_t)fd);
    if (ufd < HUGE_FD)
    {
        assert(-1 == _fd_index[ufd]);
        _fd_index[ufd] = index;
    }
    else
    {
        _fd_index_huge[fd] = index;
    }

    return index;
}

void PollBackend::update_fd_index(int32_t fd, int32_t index)
{
    uint32_t ufd = ((uint32_t)fd);
    if (ufd < HUGE_FD)
    {
        _fd_index[ufd] = index;
    }
    else
    {
        _fd_index_huge[fd] = index;
    }
}

int32_t PollBackend::modify_fd(int32_t fd, int32_t op, int32_t new_ev)
{
    // 当前禁止修改_poll_fd数组
    assert(!_modify_protected);

    int32_t index;
    if (op == FD_OP_DEL)
    {
        index = del_fd_index(fd);
        // 不如果不是数组的最后一个，则用数组最后一个位置替换当前位置，然后删除最后一个
        int32_t fd_count = (int32_t)_poll_fd.size() - 1;
        if (EXPECT_TRUE(index < fd_count))
        {
            auto &poll_fd   = _poll_fd[fd_count];
            _poll_fd[index] = poll_fd;
            update_fd_index((int32_t)poll_fd.fd, index);
        }
        _poll_fd.pop_back();
        return 0;
    }

    if (op == FD_OP_ADD)
    {
        index = add_fd_index(fd);
        _poll_fd.resize(index + 1);
        _poll_fd[index].fd = fd;
    }
    else
    {
        index = get_fd_index(fd);
    }
    assert(_poll_fd[index].fd == fd);

    int32_t events =
        ((new_ev & EV_READ || new_ev & EV_ACCEPT) ? (int32_t)POLLIN : 0)
        | ((new_ev & EV_WRITE || new_ev & EV_CONNECT) ? (int32_t)POLLOUT : 0);

    _poll_fd[index].events = (int16_t)events;

    return 0;
}
