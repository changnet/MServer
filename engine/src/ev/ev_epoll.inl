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
class FinalBackend final : public EVBackend
{
public:
    FinalBackend();
    ~FinalBackend();

    bool stop();
    bool start(class EV *ev);
    void wake();
    void backend();
    void modify(int32_t fd, EVIO *w);

private:
    /// 处理epoll事件
    int32_t do_event(int32_t ev_count);
    /// 处理主线程发起的io事件
    void do_action(const std::vector<int32_t> &actions);

    /**
     * @brief 处理读写后的io状态
     * @param w 待处理的watcher
     * @param ev 当前执行的事件
     * @param status 待处理的状态
     * @return 是否继续执行
    */
    bool do_io_status(EVIO *w, int32_t ev, const IO::IOStatus &status);

    void do_modify();

    /// 把一个io设置到epoll内核中，该函数外部需要加锁
    int32_t modify_one(int32_t fd, EVIO *w, int32_t new_ev = 0);

private:
    std::thread _thread;

    int32_t _ep_fd;
    int32_t _wake_fd; /// 用于唤醒子线程的fd
    epoll_event _ep_ev[EPOLL_MAXEV];

    /// 等待变更到epoll的fd
    std::vector<int32_t> _modify_fd;
};

FinalBackend::FinalBackend()
{
}

FinalBackend::~FinalBackend()
{
}

bool FinalBackend::start(class EV *ev)
{
    // 创建一个fd用于实现self pipe，唤醒io线程
    _wake_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (_wake_fd < 0)
    {
        ELOG("fail to create eventfd e = %d: %s", errno, strerror(errno));
        return false;
    }

    EVBackend::start(ev);
    _thread = std::thread(&FinalBackend::backend, this);

    return true;
}

bool FinalBackend::stop()
{
    EVBackend::stop();

    _thread.join();

    ::close(_wake_fd);
    _wake_fd = -1;

    return true;
}

bool FinalBackend::do_io_status(EVIO *w, int32_t ev, const IO::IOStatus &status)
{
    switch (status)
    {
    case IO::IOS_OK:
        // 发送完则需要删除写事件，不然会一直触发
        if (EV_WRITE == ev && (w->_extend_ev & EV_WRITE))
        {
            w->_extend_ev &= ~EV_WRITE;
            modify_one(w->_fd, w);
        }
        return true;
    case IO::IOS_READ:
        // socket一般都会监听读事件，不需要做特殊处理
        return true;
    case IO::IOS_WRITE:
        // 未发送完，加入write事件继续发送
        if (!(w->_extend_ev & EV_WRITE))
        {
            w->_extend_ev |= EV_WRITE;
            modify_one(w->_fd, w);
        }
        return true;
    case IO::IOS_BUSY:
        // 主线程忙不过来了，子线程需要sleep一下等待主线程
        _busy = true;
        return true;
    case IO::IOS_CLOSE:
    case IO::IOS_ERROR: 
        // TODO 错误时是否要传一个错误码给主线程
        _ev->io_event(w, EV_CLOSE);
        modify_one(w->_fd, nullptr);
        return false;
    default:
        ELOG("unknow io status: %d", status);
        return false;
    }

    return false;
}

void FinalBackend::do_action(const std::vector<int32_t> &actions)
{
    for (auto fd : actions)
    {
        EVIO *w = _ev->get_fast_io(fd);
        // 主线程发出io请求后，后续逻辑可能删掉了这个对象，但没清掉action
        if (!w) continue;

        int32_t events = 0;
        {
            std::lock_guard<SpinLock> lg(_ev->fast_lock());
            events = w->_action_ev;
            w->_action_index = 0;
        }

        // 主线程设置事件的时候，可能刚好被io线程处理完了
        // 极限情况下，主线程的fd是被断开又重连，根本不是同一个io对象
        if (0 == events) continue;

        if (events & EV_ACCEPT)
        {
            // 初始化新socket，只有ssl用到
            w->do_init_accept();
        }
        else if (events & EV_CONNECT)
        {
            // 初始化新socket，只有ssl用到
            w->do_init_connect();
        }

        // 处理数据发送
        if (events & EV_WRITE)
        {
            auto status = w->send();
            do_io_status(w, EV_WRITE, status);
        }
    }
}

int32_t FinalBackend::do_event(int32_t ev_count)
{
    int32_t use_ev = 0;
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
            continue;
        }

        EVIO *w = _ev->get_fast_io(fd);
        // io对象可能被主线程删了或者停止
        if (!w) continue;

        int32_t set_ev = ev->data.u32;
        int32_t events = 0;
        if (ev->events & (EPOLLOUT | EPOLLERR | EPOLLHUP))
        {
            if (set_ev & EV_WRITE)
            {
                events |= EV_WRITE;
                auto status = w->send();
                do_io_status(w, EV_WRITE, status);
            }
            else
            {
                events |= EV_CONNECT;
            }
        }
        if (ev->events & (EPOLLIN | EPOLLERR | EPOLLHUP))
        {
            if (set_ev & EV_READ)
            {
                events |= EV_READ;

                auto status = w->recv();
                do_io_status(w, EV_READ, status);
            }
            else
            {
                events |= EV_ACCEPT;
            }
        }
        if (set_ev & events)
        {
            ++use_ev;
            _ev->io_event(w, events);
        }
        else
        {
            // epoll收到事件，但没有触发io_event，一般是io线程自己添加写事件来发送数据
            // 如果有其他事件，应该是哪里出错了
            assert(events == EV_WRITE);
        }
    }

    return use_ev;
}

void FinalBackend::backend()
{
#ifdef EPOLL_CLOEXEC
    _ep_fd = epoll_create1(EPOLL_CLOEXEC);

    if (_ep_fd < 0 && (errno == EINVAL || errno == ENOSYS))
#endif
    {
        _ep_fd = epoll_create(256);
        if (_ep_fd < 0 || fcntl(_ep_fd, F_SETFD, FD_CLOEXEC) < 0)
        {
            FATAL("epoll_create init fail:%s", strerror(errno));
            return;
        }
    }
    assert(_ep_fd >= 0);

    if ( 0 != modify_one(_wake_fd, nullptr, EV_READ))
    {
        FATAL("backend init wake_fd fail:%s", strerror(errno));
        return;
    }

    /// 第一次进入wait前，必须先设置io事件，不然的话第一次就会wait很久
    {
        std::lock_guard<std::mutex> lg(_ev->lock());
        do_modify();
    }

    std::vector<int32_t> actions;
    actions.reserve(1024);

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

            _ev->quit();
            break;
        }

        // 主线程和io线程会频繁交换action,因此使用一个快速、独立的锁
        {
            actions.clear();
            std::lock_guard<SpinLock> lg(_ev->fast_lock());
            actions.swap(_ev->get_io_action());
        }

        // TODO 下面的操作，是每个操作，加锁、解锁一次，还是全程解锁呢？
        // 即使全程加锁，至少是不会影响主线程执行逻辑的。主线程执行逻辑回调时，不会用到锁
        int32_t use_ev = 0;
        {
            std::lock_guard<std::mutex> lg(_ev->lock());

            do_action(actions);
            use_ev = do_event(ev_count);
            do_modify();

            if (use_ev > 0) _ev->set_job(true);
        }

        // 唤醒主线程干活
        // https://en.cppreference.com/w/cpp/thread/condition_variable
        // the lock does not need to be held for notification
        if (use_ev > 0) _ev->wake();

        // TODO 检测缓冲区，如果主线程处理不过来，这里sleep一下
    }

    ::close(_ep_fd);
    _ep_fd = -1;
}

void FinalBackend::wake()
{
    static const int64_t v = 1;
    if (::write(_wake_fd, &v, sizeof(v)) <= 0)
    {
        ELOG("fail to wakeup epoll e = %d: %s", errno, strerror(errno));
    }
}

void FinalBackend::modify(int32_t fd, EVIO *w)
{
    /**
     * 这个函数由主线程调用，io线程可能处于wpoll_wait阻塞当中
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

    if (w) w->_emask = w->_events;

    _modify_fd.push_back(fd);
}

void FinalBackend::do_modify()
{
    for (auto &fd : _modify_fd)
    {
        EVIO *w = _ev->get_fast_io(fd);
        modify_one(fd, w);
    }
    _modify_fd.clear();
}

int32_t FinalBackend::modify_one(int32_t fd, EVIO *w, int32_t new_ev)
{
    struct epoll_event ev;
    /* valgrind: uninitialised byte(s) */
    memset(&ev, 0, sizeof(ev));

    ev.data.fd  = fd;

    int32_t old_ev = 0;
    if (w)
    {
        old_ev = w->_kernel_ev;
        new_ev = w->_emask | w->_extend_ev;

        w->_kernel_ev = new_ev;
    }

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
        // 同一次主循环内，想把一个fd添加到epoll，但在真正执行之前又从epoll删掉
        if (!old_ev) return 0;
    }

    if (EXPECT_TRUE(!epoll_ctl(_ep_fd, op, fd, &ev))) return 0;

    switch (errno)
    {
    case EBADF:
        /* libev是不处理EPOLL_CTL_DEL的，因为fd被关闭时会自动从epoll中删除
         * 这里我们允许不关闭fd而从epoll中删除，但是执行删除时会遇到该fd已
         * 被epoll自动删除，这里特殊处理
         */
        if (EPOLL_CTL_DEL == op) return 0;
        assert(false);
        break;
    case EEXIST:
        /*
         * 上一个fd关闭了，系统又分配了同一个fd
         */
        if (old_ev == new_ev) return 0;
        if (EXPECT_TRUE(!epoll_ctl(_ep_fd, EPOLL_CTL_MOD, fd, &ev)))
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
        if (EXPECT_TRUE(!epoll_ctl(_ep_fd, EPOLL_CTL_ADD, fd, &ev)))
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
