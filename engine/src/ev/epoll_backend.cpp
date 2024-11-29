/*
 * for epoll
 * 1.closing a file descriptor cause it to be removed from all
 *   poll sets automatically。（not dup fork,see man page)
 * 2.EPOLL_CTL_MOD操作可能会使得某些event被忽略(如当前有已有一个读事件，但忽然改为只监听
 *   写事件)，可能会导致这个事件丢失，尤其是使用ET模式时。
 * 3.采用pending、fdchanges、anfds队列，同时要保证在处理pending时(此时会触发很多事件)，不会
 *   导致其他数据指针悬空(比如delete某个watcher导致anfds指针悬空)。
 * 4.与libev的自动调整不同，单次poll的事件数量是固定的。因此，当io事件很多时，可能会导致
 *   epoll_wait数组一次装不下，可以有某个fd永远未被读取(ET模式下将会导致下次不再触发)。
 *   这时需要调整事件大小，重新编译。
 */

#include "epoll_backend.hpp"
#if defined(__epoll__)

#include <fcntl.h>
#include <unistd.h> /* POSIX api, like close */
#include <sys/eventfd.h> // for eventfd

const char *__BACKEND__ = "epoll";

EpollBackend::EpollBackend()
{
    ep_fd_   = -1;
    wake_fd_ = -1;
}

EpollBackend::~EpollBackend()
{
}

bool EpollBackend::before_start()
{
    // 创建epoll
#ifdef EPOLL_CLOEXEC
    ep_fd_ = epoll_create1(EPOLL_CLOEXEC);

    if (ep_fd_ < 0 && (errno == EINVAL || errno == ENOSYS))
#endif
    {
        ep_fd_ = epoll_create(256);
        if (ep_fd_ < 0 || fcntl(ep_fd_, F_SETFD, FD_CLOEXEC) < 0)
        {
            FATAL("epoll_create init fail:%s", strerror(errno));
            return false;
        }
    }

    // 创建一个fd用于实现self pipe，唤醒io线程
    wake_fd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (wake_fd_ < 0)
    {
        ELOG("fail to create eventfd e = %d: %s", errno, strerror(errno));
        return false;
    }

    // 把self pipe fd放到epoll
    if (0 != modify_fd(wake_fd_, EPOLL_CTL_ADD, EV_READ))
    {
        FATAL("epoll init wake_fd fail:%s", strerror(errno));
        return false;
    }

    return true;
}

void EpollBackend::after_stop()
{
    ::close(wake_fd_);
    wake_fd_ = -1;

    ::close(ep_fd_);
    ep_fd_ = -1;
}

void EpollBackend::do_wait_event(int32_t ev_count)
{
    for (int32_t i = 0; i < ev_count; ++i)
    {
        struct epoll_event *ev = ep_ev_ + i;

        int32_t fd = ev->data.fd;
        if (fd == wake_fd_)
        {
            int64_t v = 0;
            if (::read(fd, &v, sizeof(v)) < 0)
            {
                ELOG("read epoll wakeup fd error e = %d: %s", errno,
                     strerror(errno));
                // 避免出错日志把硬盘刷爆
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
            continue;
        }

        int32_t revents = ev->events;

        int32_t events = 0;
        if (revents & EPOLLOUT) events |= EV_WRITE;
        if (revents & EPOLLIN) events |= EV_READ;

        // EPOLLERR | EPOLLHUP和EPOLLOUT | EPOLLIN是同时存在的
        // 但有时候watcher并没有设置EV_WRITE或者EV_READ，因此需要独立检测
        if (revents & (EPOLLERR | EPOLLHUP)) events |= EV_CLOSE;

        EVIO *w = fd_mgr_.get(static_cast<int32_t>(fd));
        assert(w);
        do_watcher_wait_event(w, events);
    }
}

void EpollBackend::wake()
{
    static const int64_t v = 1;
    if (::write(wake_fd_, &v, sizeof(v)) <= 0)
    {
        assert(wake_fd_ > 0);
        ELOG("fail to wakeup epoll e = %d: %s", errno, strerror(errno));
    }
}

int32_t EpollBackend::wait(int32_t timeout)
{
    int32_t ev_count = epoll_wait(ep_fd_, ep_ev_, EPOLL_MAXEV, timeout);
    if (unlikely(ev_count < 0))
    {
        if (errno == EINTR) return 0;

        FATAL("epoll_wait errno(%d)", errno);
        ev_->quit();
    }

    return ev_count;
}

int32_t EpollBackend::modify_fd(int32_t fd, int32_t op, int32_t new_ev)
{
    struct epoll_event ev;
    /* valgrind: uninitialised byte(s) */
    memset(&ev, 0, sizeof(ev));

    ev.data.fd = fd;

    /* epoll只是监听fd，不负责fd的打开或者关闭。
     * 但是，如果一个fd被关闭，epoll会自动解除监听，并不会通知我们。
     * 因此，在处理EPOLL_CTL_DEL时，返回EBADF表明是自动解除。或者在epoll_ctl之前
     * 调用fcntl(fd,F_GETFD)判断fd是否被关闭，但效率与直接调用epoll_ctl一样。
     * libev和libevent均采用类似处理方法。但它们由于考虑dup、fork、thread等特性，
     * 处理更为复杂。
     */

    /**
     * LT(Level Triggered)同时支持block和no-block，持续通知
     * ET(Edge Trigger)只支持no-block，一个事件只通知一次
     * epoll默认是LT模式
     */
    ev.events =
        ((new_ev & EV_READ || new_ev & EV_ACCEPT) ? (int32_t)EPOLLIN : 0)
        | ((new_ev & EV_WRITE || new_ev & EV_CONNECT) ? (int32_t)EPOLLOUT
                                                      : 0) /* | EPOLLET */;

    /**
     * ev.events可能为0。例如一个socket连接时只监听EV_CONNECT，第一次触发后会
     * 删除掉EV_CONNECT，则ev.events为0
     *
     * 即使ev.events为0，epoll还是会监听EPOLLERR和EPOLLHUP，可以处理socket的关闭
     */

    if (likely(!epoll_ctl(ep_fd_, op, fd, &ev))) return 0;

    switch (errno)
    {
    case EBADF:
        /* fd被关闭时会自动从epoll中删除
         * 当主动关闭fd时，会恰好遇到该fd已被epoll自动删除，这里特殊处理
         */
        if (EPOLL_CTL_DEL == op) return 0;
        assert(false);
        break;
    case EEXIST:
        /*
         * op的值是根据ev中保存的数据结构计算出来的。但假如ev中的数据已经重置了
         * 但epoll线程还来不及关闭fd。这时ev中再次添加了该watcher的事件，就会被
         * 标记为EPOLL_CTL_ADD操作，这里重新修正为mod操作
         */
        if (likely(!epoll_ctl(ep_fd_, EPOLL_CTL_MOD, fd, &ev)))
        {
            return 0;
        }
        assert(false);
        break;
    case EINVAL: assert(false); break;
    case ENOENT:
        /* ENOENT：fd不属于这个backend_fd管理的fd
         * epoll在连接断开时，会把fd从epoll中删除，但ev中仍保留相关数据
         * 如果这时在同一轮主循环中产生新连接，系统会分配同一个fd
         * 由于ev中仍有旧数据，会被认为是EPOLL_CTL_MOD,这里修正为ADD
         */
        if (EPOLL_CTL_DEL == op) return 0;
        if (likely(!epoll_ctl(ep_fd_, EPOLL_CTL_ADD, fd, &ev)))
        {
            return 0;
        }
        assert(false);
        break;
    case ENOMEM: assert(false); break;
    case ENOSPC: assert(false); break;
    case EPERM:
        // 一个fd被epoll自动删除后，可能会被分配到其他用处，比如打开了个文件，不是有效socket
        if (EPOLL_CTL_DEL == op) return 0;
        assert(false);
        break;
    default: ELOG("unknow epoll error"); break;
    }

    return errno;
}
#endif
