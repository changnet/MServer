#include "../net/socketpair.hpp"

#ifdef __windows__
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>

    const char *__BACKEND__ = "WSAPoll";
#else
    #include <poll.h>
    const char *__BACKEND__ = "poll";
#endif

// 是否要使用wepoll替换
// https://github.com/piscisaureus/wepoll
// wepoll使用的机制 NtDeviceIoControlFile 是一个未公开的机制
// 它使用IOCTL_AFD_POLL标记来实现类似epoll_ctl的功能

/// backend using poll implement
class FinalBackend final : public EVBackend
{
public:
    FinalBackend();
    ~FinalBackend();

    void after_stop() override;
    bool before_start() override;
    void wake() override;

private:
    int32_t wait(int32_t timeout) override;
    void do_wait_event(int32_t ev_count) override;
    int32_t modify_fd(int32_t fd, int32_t op, int32_t new_ev) override;

private:
    int32_t _wake_fd[2]; /// 用于唤醒子线程的fd
    std::vector<int32_t> _fd_index;

#ifdef __windows__
    std::vector<WSAPOLLFD> _poll_fd;
#else
    std::vector<struct pollfd> _poll_fd;
#endif
};

FinalBackend::FinalBackend() : _fd_index(1024, -1)
{
    _wake_fd[0] = _wake_fd[1] = -1;
    _poll_fd.reserve(1024);
}

FinalBackend::~FinalBackend()
{
}

void FinalBackend::after_stop()
{
#ifdef __windows__
    if (_wake_fd[0] >= 0) ::closesocket(_wake_fd[0]);
    if (_wake_fd[1] >= 0) ::closesocket(_wake_fd[1]);
#else
    if (_wake_fd[0] >= 0) ::close(_wake_fd[0]);
    if (_wake_fd[1] >= 0) ::close(_wake_fd[1]);
#endif
}

bool FinalBackend::before_start()
{
    if (socketpair(AF_LOCAL, SOCK_STREAM, 0, _wake_fd) < 0)
    {
#ifdef __windows__
        ELOG("poll init socketpair fail: %d", WSAGetLastError());
#else
        ELOG("poll init socketpair fail: %d", errno);
#endif
        return false;
    }

    if (modify_fd(_wake_fd[1], FD_OP_ADD, EV_READ) < 0)
    {
        return false;
    }

    return true;
}

void FinalBackend::wake()
{
    const int8_t v = 1;
    ::send(_wake_fd[0], &v, sizeof(v), 0);
}

void FinalBackend::do_wait_event(int32_t ev_count)
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
            static char buffer[512];
            while (true)
            {
                ssize_t byte = ::recv(fd, buffer, sizeof(buffer), 0);

                // 这个连接在程序运行时应该永远不会关闭
                assert(byte > 0);
                if ((size_t)byte < sizeof(buffer)) break;
            }
        }
        else
        {
            int32_t events = 0;
            if (revents & POLLOUT) events |= EV_WRITE;
            if (revents & POLLIN) events |= EV_READ;
            if (revents & (POLLERR | POLLHUP)) events |= EV_CLOSE;

            EVIO *w = _ev->get_fast_io(fd);
            assert(w);
            do_watcher_wait_event(w, events);
        }

        // poll返回值表示数组中有N个fd有事件，但只能遍历来查找哪个有事件
        ev_count--;
        if (0 == ev_count) break;
    }
}

int32_t FinalBackend::wait(int32_t timeout)
{
    /**
     * WSAPoll有个bug，connect失败不会触发事件，直到win10 2004才修复
     * https://stackoverflow.com/questions/21653003/is-this-wsapoll-bug-for-non-blocking-sockets-fixed
     * https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsapoll
     * As of Windows 10 version 2004, when a TCP socket fails to connect, (POLLHUP \| POLLERR \| POLLWRNORM) is indicated
    */

#ifdef __windows__
    int32_t ev_count = WSAPoll(_poll_fd.data(), _poll_fd.size(), timeout);
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
            ELOG("poll fatal, %s(%d)", strerror(errno), errno);
            assert(false);
            return -1;
        }
    }

    return ev_count;
}

int32_t FinalBackend::modify_fd(int32_t fd, int32_t op, int32_t new_ev)
{
    if (EXPECT_FALSE(_fd_index.size() < size_t(fd + 1)))
    {
        _fd_index.resize(fd + 1, -1);
    }

    int32_t index = _fd_index[fd];
    if (index < 0)
    {
        _fd_index[fd] = index = (int32_t)_poll_fd.size();
        _poll_fd.resize(index + 1);
        _poll_fd[index].fd = fd;
    }
    assert(_poll_fd[index].fd == fd);

    if (op != FD_OP_DEL)
    {
        _poll_fd[index].events = (int16_t)
            ((new_ev & EV_READ ? POLLIN : 0) | (new_ev & EV_WRITE ? POLLOUT : 0));
    }
    else
    {
        _fd_index[fd] = -1;
        // 不如果不是数组的最后一个，则用数组最后一个位置替换当前位置，然后删除最后一个
        int32_t fd_count = (int)_poll_fd.size();
        if (EXPECT_TRUE(index < --fd_count))
        {
            _poll_fd[index]               = _poll_fd[fd_count];
            _fd_index[_poll_fd[index].fd] = index;
        }
        _poll_fd.pop_back();
    }

    return 0;
}
