#include "../net/socketpair.hpp"
#include "../net/net_compat.hpp"

#ifdef __windows__
    #include <SdkDdkVer.h>

    const char *__BACKEND__ = "WSAPoll";
#else
    #include <poll.h>
    const char *__BACKEND__ = "poll";
#endif

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
    if (_wake_fd[0] != netcompat::INVALID) netcompat::close(_wake_fd[0]);
    if (_wake_fd[1] != netcompat::INVALID) netcompat::close(_wake_fd[1]);
}

bool FinalBackend::before_start()
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
        int32_t e = netcompat::noerror();
        ELOG("poll init socketpair fail(%d): %s", e, netcompat::strerror(e));

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
    static const int8_t v = 1;
    ::send(_wake_fd[0], (const char *)&v, sizeof(v), 0);
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

            EVIO *w = get_fd_watcher(static_cast<int32_t>(fd));
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
            int32_t e = netcompat::noerror();
            ELOG("poll fatal(%d): %s", e, netcompat::strerror(e));
            assert(false);
            return -1;
        }
        }
    }

    return ev_count;
}

int32_t FinalBackend::modify_fd(int32_t fd, int32_t op, int32_t new_ev)
{
    // 当前禁止修改_poll_fd数组
    assert(!_modify_protected);
    // 限制fd大小，避免_fd_index分配过大内存
    assert(fd >= 0 && fd < 102400);
    if (EXPECT_FALSE(_fd_index.size() < size_t(fd + 1)))
    {
        _fd_index.resize(fd + 1024, -1);
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
        int32_t events =
            ((new_ev & EV_READ || new_ev & EV_ACCEPT) ? (int32_t)POLLIN : 0)
            | ((new_ev & EV_WRITE || new_ev & EV_CONNECT) ? (int32_t)POLLOUT : 0);

        _poll_fd[index].events = (int16_t) events;
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
