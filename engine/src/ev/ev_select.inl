#include "../global/platform.hpp"
#ifdef __windows__
    // 如果要修改select支持的socket数量，编译的时候定义FD_SETSIZE的大小
    #include <winsock2.h>
#else
    #include <sys/select.h>
using SOCKET = int32_t // 兼容windows代码
#endif

#include "ev_watcher.hpp"

const char *__BACKEND__ = "select";

/**
 * 为什么使用select?
 * Linux下不用这个的，这个主要是兼容windows下测试用的
 *
 * 为什么不用IOCP？
 * IOCP是启动多个线程，然后给触发事件的fd分配一个线程去读、写，这和当前的事件模型有比较大的区别,
 * 它无法提供各个文件描述符的事件。目前框架用的是Reactor模式，而IOCP其实适合Proactor模式。
 * 参考：https://libevent-users.monkey.narkive.com/XWWaQ6ao/integrate-windows-i-o-completion-port-into-libevent
 *
 * WSAPoll？
 * 这个好像没比select好到哪去，而且其他系统还不通用
 *
 * select的效率
 * 在连接比较少的时候，还是很高效的。目前也只是用来做兼容，生产环境并不打算用这个，不用太在意效率
 * 如果需要在win做生产环境，考虑下：https://github.com/piscisaureus/wepoll
 * win下还有一个来源于gnulib的poll模拟，可以直接使用，不过是GPL协议。
 * git有使用这个库：https://github.com/git/git/blob/master/compat/poll/poll.h
 *
 * select的限制
 * https://moythreads.com/wordpress/2009/12/22/select-system-call-limitation/
 * /usr/include/sys/select.h
 * typedef struct  {
 *   long int fds_bits[32];//32bit platform
 * } fd_set;
 * select在linux下只能处理小于1024的文件描述符。不仅仅是能处理的文件描述符数量小于1024，文件
 * 描述符的值本身也不能大于1024，因为1025无法按位放到fds_bits[32]这个结构中。绕过这个限制的方
 * 式，即不使用fd_set这个结构，而是自己维护一个可变数组。这种方式可行，但有一定的风险，毕竟glibc
 * 中的fds_bits是固定的，无法保证所有调用的情况。libev(ev_select.c)和libevent都使用了这种方
 * 式来绕过限制。
 *
 * https://docs.microsoft.com/en-us/windows/win32/winsock/maximum-number-of-sockets-supported-2
 * win下，默认是64，但是可以通过重定义FD_SETSIZE修改
 * The default value in Winsock2.h is 64. If an application is designed to be
 * capable of working with more than 64 sockets using the select and WSAPoll
 * functions, the implementor should define the manifest FD_SETSIZE in every
 * source file before including the Winsock2.h header file
 */

/// backend using select implement
class EVBackend final
{
public:
    EVBackend();
    ~EVBackend();

    void wait(class EV *ev_loop, int64_t timeout);
    void modify(int32_t fd, int32_t old_ev, int32_t new_ev);

private:
    fd_set _ex_fd_set; /// exception fd set
    fd_set _ri_fd_set; /// read input
    fd_set _wi_fd_set; /// write input
    fd_set _ro_fd_set; /// read output
    fd_set _wo_fd_set; /// write output
};

EVBackend::EVBackend()
{
    FD_ZERO(&_ri_fd_set);
    FD_ZERO(&_wi_fd_set);
}

EVBackend::~EVBackend() {}

void EVBackend::wait(class EV *ev_loop, int64_t timeout)
{
    size_t size = sizeof(fd_set);

    // 执行select的时候，会改写_xx_fd_set中的数据，因此需要复制一份
    memcpy(&_ro_fd_set, &_ri_fd_set, size);
    memcpy(&_wo_fd_set, &_wi_fd_set, size);

    // win下，如果connect失败，需要通过exception来取得write事件
    // https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-select
    // exceptfds:
    // If processing a connect call (nonblocking), connection attempt failed
    fd_set *ex_fd_set = nullptr;
#ifdef __windows__
    ex_fd_set = &_ex_fd_set;
    memcpy(ex_fd_set, &_wi_fd_set, size);
#endif

    struct timeval tv;
    tv.tv_sec  = long(timeout / 1000);
    tv.tv_usec = long((timeout % 1000) * 1000);
    int32_t ok = select((int32_t)size, &_ro_fd_set, &_wo_fd_set, ex_fd_set, &tv);
    if (EXPECT_FALSE(ok < 0))
    {
#ifdef __windows__
        // https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-select
        errno = WSAGetLastError();
        switch (errno)
        {
        case WSAEINTR: return;
        case WSANOTINITIALISED: FATAL("WSAStartup not call"); return;
        case WSAEINVAL:
            // 当需要监听的socket数量为0时，会触发这个错误
            // The time-out value is not valid, or all three descriptor parameters were null.
            Sleep((DWORD)timeout);
            return;
        default: ELOG("select backend error(%d):%s", errno, strerror(errno));
        }
#else
        // https://man7.org/linux/man-pages/man2/select.2.html
        if (errno == EINTR) return;
        // 其他错误，EBADF、EINVAL、ENOMEM除了打个日志，没啥可以处理的了
        ELOG("select backend error(%d):%s", errno, strerror(errno));
#endif
        return;
    }

    for (auto &watcher : ev_loop->_fds)
    {
        // 只用设置过mask的才会在select的数组中
        if (!watcher->get_mask()) continue;

        int32_t events = 0;
        int32_t fd     = watcher->get_fd();
        if (FD_ISSET(fd, &_ro_fd_set)) events |= EV_READ;
        if (FD_ISSET(fd, &_wo_fd_set)) events |= EV_WRITE;
        if (ex_fd_set && FD_ISSET(fd, ex_fd_set)) events |= EV_WRITE;
        if (events) ev_loop->fd_event(fd, events);
    }
}

void EVBackend::modify(int32_t fd, int32_t old_ev, int32_t new_ev)
{
#ifndef __windows__
    assert(fd < FD_SETSIZE);
#endif

    // 在win下fd_set是一个数组，FD_SET占一个元素位置，多次设置会导致同一个fd占用多个位置
    // linux下，fd_set是一个按bit设置的数组，多次设置同一个fd并没有问题
    if ((new_ev & EV_READ) && !(old_ev & EV_READ))
    {
        FD_SET((SOCKET)fd, &_ri_fd_set);
    }
    else if ((old_ev & EV_READ) && !(new_ev & EV_READ))
    {
        FD_CLR((SOCKET)fd, &_ri_fd_set);
    }
    if ((new_ev & EV_WRITE) && !(old_ev & EV_WRITE))
    {
        FD_SET((SOCKET)fd, &_wi_fd_set);
    }
    else if ((old_ev & EV_WRITE) && !(new_ev & EV_WRITE))
    {
        FD_CLR((SOCKET)fd, &_wi_fd_set);
    }
}
