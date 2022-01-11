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

#include <fcntl.h>
#include <unistd.h> /* POSIX api, like close */
#include <sys/epoll.h>
#include <sys/eventfd.h> // for eventfd

#include <thread>
#include <chrono> // for 1000ms

#include "ev.hpp"
#include "ev_backend.hpp"

const char *__BACKEND__ = "epoll";

/// epoll max events one poll
/* https://man7.org/linux/man-pages/man2/epoll_wait.2.html
 * If more than maxevents file descriptors are ready when
 * epoll_wait() is called, then successive epoll_wait() calls will
 * round robin through the set of ready file descriptors.  This
 * behavior helps avoid starvation scenarios
 */
static const int32_t EPOLL_MAXEV = 8192;

/// backend using epoll implement
class EVEPoll final : public EVBackend
{
public:
    EVEPoll();
    ~EVEPoll();

    bool stop();
    bool start(class EV *ev);
    void wake();
    void backend();
    void modify(int32_t fd, int32_t old_ev, int32_t new_ev);

private:
    void do_modify();
    void modify_one(int32_t fd, int32_t old_ev, int32_t new_ev);

private:
    std::thread _thread;

    int32_t _ep_fd;
    int32_t _wake_fd; /// 用于唤醒子线程的fd
    epoll_event _ep_ev[EPOLL_MAXEV];

    /// 等待变更到epoll的io事件
    std::vector<struct ModifyEvent> _modify_event;
};

EVEPoll::EVEPoll()
{
}

EVEPoll::~EVEPoll()
{
}

bool EVEPoll::start(class EV *ev)
{
    _wake_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (_wake_fd < 0)
    {
        ELOG("fail to create eventfd e = %d: %s", errno, strerror(errno));
        return false;
    }

    EVBackend::start(ev);
    _thread = std::thread(&EVEPoll::backend, this);

    modify(_wake_fd, 0, EV_READ);

    return true;
}

bool EVEPoll::stop()
{
    EVBackend::stop();

    _thread.join();

    ::close(_wake_fd);
    _wake_fd = -1;

    return true;
}

void EVEPoll::backend()
{
#ifdef EPOLL_CLOEXEC
    _ep_fd = epoll_create1(EPOLL_CLOEXEC);

    if (_ep_fd < 0 && (errno == EINVAL || errno == ENOSYS))
#endif
    {
        _ep_fd = epoll_create(256);
        if (_ep_fd < 0 || fcntl(_ep_fd, F_SETFD, FD_CLOEXEC) < 0)
        {
            FATAL("ev backend init fail:%s", strerror(errno));
            return;
        }
    }
    assert(_ep_fd >= 0);

    /// 第一次进入wait前，必须先设置io事件，不然的话第一次就会wait很久
    _ev->lock();
    do_modify();
    _ev->unlock();

    while (!_done)
    {
        int32_t ev_count =
            epoll_wait(_ep_fd, _ep_ev, EPOLL_MAXEV, (int32_t)BACKEND_MAX_TM);
        if (EXPECT_FALSE(ev_count < 0))
        {
            if (errno != EINTR)
            {
                FATAL("epoll_wait errno(%d)", errno);
            }

            break;
        }

        /// 下面的操作，速度很快，可以加锁
        _ev->lock();
        for (int32_t i = 0; i < ev_count; ++i)
        {
            struct epoll_event *ev = _ep_ev + i;

            int32_t fd = ev->data.fd;
            if (fd == _wake_fd)
            {
                int64_t v = 0;
                if (::read(fd, &v, sizeof(v)) < 0)
                {
                    ELOG("read epoll wakeup fd error e = %d: %s", errno,
                         strerror(errno));
                    // 避免出错日志把硬盘刷爆
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }
            }
            else
            {
                int32_t events = 0;
                EVIO *w        = _ev->get_fast_io(fd); // io对象可能被主线程删了
                if (ev->events & (EPOLLOUT | EPOLLERR | EPOLLHUP))
                {
                    events |= EV_WRITE;
                    if (w) w->write();
                }
                if (ev->events & (EPOLLIN | EPOLLERR | EPOLLHUP))
                {
                    events |= EV_WRITE;
                    if (w) w->read();
                }
                _ev->io_event(w, events);
            }
        }
        do_modify();
        _ev->unlock();

        // 唤醒主线程干活
        // https://en.cppreference.com/w/cpp/thread/condition_variable
        // the lock does not need to be held for notification
        if (ev_count > 0) _ev->wake();

        // TODO 检测缓冲区，如果主线程处理不过来，这里sleep一下
    }

    ::close(_ep_fd);
    _ep_fd = -1;
}

void EVEPoll::wake()
{
    static const int64_t v = 1;
    if (::write(_wake_fd, &v, sizeof(v)) <= 0)
    {
        ELOG("fail to wakeup epoll e = %d: %s", errno, strerror(errno));
    }
}

void EVEPoll::modify(int32_t fd, int32_t old_ev, int32_t new_ev)
{
    /**
     * man epoll_wait
     * NOTES
       While one thread is blocked in a call to epoll_pwait(), it is possible
     for another thread to add a file  descriptor  to the waited-upon epoll
     instance.  If the new file descriptor becomes ready, it will cause the
     epoll_wait() call to unblock.

       For a discussion of what may happen if a file descriptor in an epoll
     instance being  monitored  by epoll_wait() is closed in another thread, see
     select(2).

     * man select
     * Multithreaded applications
       If a file descriptor being monitored by select() is closed in another
     thread, the  result  is  un‐ specified.   On some UNIX systems, select()
     unblocks and returns, with an indication that the file descriptor is ready
     (a subsequent I/O operation will likely fail with an error, unless another
     the file descriptor reopened between the time select() returned and the I/O
     operations was performed). On Linux (and some other systems), closing the
     file descriptor in another thread has no effect  on select().   In summary,
     any application that relies on a particular behavior in this scenario must
       be considered buggy.

     * 所以epoll_ctl其实不是线程安全的，调用的时候，backend线程不能处于epoll_wait状态
     */

    _modify_event.emplace_back(fd, old_ev, new_ev);
}

void EVEPoll::do_modify()
{
    for (auto &e : _modify_event)
    {
        modify_one(e._fd, e._old_ev, e._new_ev);
    }
    _modify_event.clear();
}

void EVEPoll::modify_one(int32_t fd, int32_t old_ev, int32_t new_ev)
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
    ev.events = (new_ev & EV_READ ? (int32_t)EPOLLIN : 0)
                | (new_ev & EV_WRITE ? (int32_t)EPOLLOUT : 0) /* | EPOLLET */;

    /**
     * socket被关闭时，内核自动从epoll中删除。这时执行EPOLL_CTL_DEL会触发EBADF
     * 假如同一个fd被分配了，但没放到这个epoll里，则会触发ENOENT
     * 假如同一帧内，同一个fd刚好被分配，也即将放到这个epoll里
     * old_ev && old_ev != new_ev判断出来的op可能不正确，因此需要额外处理 EEXIST
     */
    int32_t op = EPOLL_CTL_DEL;
    if (new_ev)
    {
        op = (old_ev && old_ev != new_ev) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    }
    else
    {
        // 同一帧内，把一个fd添加到epoll，但在fd_reify执行之前又从epoll删掉
        if (!old_ev) return;
    }

    if (EXPECT_TRUE(!epoll_ctl(_ep_fd, op, fd, &ev))) return;

    switch (errno)
    {
    case EBADF:
        /* libev是不处理EPOLL_CTL_DEL的，因为fd被关闭时会自动从epoll中删除
         * 这里我们允许不关闭fd而从epoll中删除，但是会遇到已被epoll自动删除，这时特殊处理
         */
        if (EPOLL_CTL_DEL == op) return;
        assert(false);
        break;
    case EEXIST:
        /*
         * 上一个fd关闭了，系统又分配了同一个fd
         */
        if (old_ev == new_ev) return;
        if (EXPECT_TRUE(!epoll_ctl(_ep_fd, EPOLL_CTL_MOD, fd, &ev)))
        {
            return;
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
        if (EPOLL_CTL_DEL == op) return;
        if (EXPECT_TRUE(!epoll_ctl(_ep_fd, EPOLL_CTL_ADD, fd, &ev)))
        {
            return;
        }
        assert(false);
        break;
    case ENOMEM: assert(false); break;
    case ENOSPC: assert(false); break;
    case EPERM:
        // 一个fd被epoll自动删除后，可能会被分配到其他用处，比如打开了个文件，不是有效socket
        if (EPOLL_CTL_DEL == op) return;
        assert(false);
        break;
    default: ELOG("unknow ev error"); break;
    }
}
