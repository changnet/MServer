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

    /**
     * 派发watcher回调事件
     */
    void feed_receive_event(EVIO *w, int32_t ev)
    {
        _has_ev = true;
        _ev->io_receive_event(w, ev);
    }

private:
    /// 处理epoll事件
    void do_event(int32_t ev_count);
    /// 处理主线程发起的io事件
    void do_fast_event(std::vector<EVIO *> &fast_events);

    /**
     * @brief 冗余检测该fd是否在epoll中
     * @param fd 要检测的fd
     * @return boolean，是否在epoll中
    */
    bool try_del_epoll_fd(int32_t fd);

    /**
     * @brief 处理读写后的io状态
     * @param w 待处理的watcher
     * @param ev 当前执行的事件
     * @param status 待处理的状态
     * @return 是否继续执行
     */
    bool do_io_status(EVIO *w, int32_t ev, const IO::IOStatus &status);

    void do_modify();

    /**
     * @brief 把一个watcher设置到epoll，该函数外部需要加锁
     * @param w watcher的指针
     * @return errno
     */
    int32_t modify_watcher(EVIO *w);
    /**
     * @brief 把一个fd设置到epoll内核中，该函数外部需要加锁
     * @param fd 需要设置的文件描述符
     * @param op epoll的操作，如EPOLL_CTL_ADD
     * @param new_ev 旧事件
     * @return errno
     */
    int32_t modify_fd(int32_t fd, int32_t op, int32_t new_ev);

    /**
     * 获取从backend对象创建以来的毫秒数
     */
    int64_t get_time_interval()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - _beg_time).count();
    }

    /**
     * 删除主动关闭，等待删除的watcher
     */
    void add_pending_watcher(int32_t fd);
    /**
     * 检测主动关闭，等待删除的watcher是否超时
     */
    void check_pending_watcher(int64_t now);
    /**
     * 删除主动关闭，等待删除的watcher
     */
    void del_pending_watcher(int32_t fd, EVIO *w);

private:
    std::thread _thread;

    bool _has_ev;     /// 是否有待主线程处理的事件
    int32_t _ep_fd;   /// epoll句柄
    int32_t _wake_fd; /// 用于唤醒子线程的fd
    epoll_event _ep_ev[EPOLL_MAXEV];

    // 线程开始时间
    std::chrono::time_point<std::chrono::steady_clock> _beg_time;

    /// 等待变更到epoll的fd
    std::vector<int32_t> _modify_fd;

    // 待发送完数据后删除的watcher
    std::unordered_map<int32_t, int64_t> _pending_watcher;
};

FinalBackend::FinalBackend()
{
    _has_ev  = false;
    _ep_fd   = -1;
    _wake_fd = -1;

    _beg_time = std::chrono::steady_clock::now();
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
        if (EV_WRITE == ev)
        {
            // 发送完后主动关闭主动关闭
            if (w->_b_uevents & EV_CLOSE)
            {
                del_pending_watcher(-1, w);
            }
            else if (w->_b_eevents & EV_WRITE)
            {
                w->_b_eevents &= static_cast<uint8_t>(~EV_WRITE);
                modify_watcher(w);
            }
        }
        return true;
    case IO::IOS_READ:
        // socket一般都会监听读事件，不需要做特殊处理
        return true;
    case IO::IOS_WRITE:
        // 未发送完，加入write事件继续发送
        if (!(w->_b_eevents & EV_WRITE))
        {
            w->_b_eevents |= EV_WRITE;
            modify_watcher(w);
        }
        return true;
    case IO::IOS_BUSY:
        // 主线程忙不过来了，子线程需要sleep一下等待主线程
        _busy = true;
        return true;
    case IO::IOS_CLOSE:
    case IO::IOS_ERROR:
        // TODO 错误时是否要传一个错误码给主线程
        del_pending_watcher(-1, w);
        return false;
    default: ELOG("unknow io status: %d", status); return false;
    }

    return false;
}

void FinalBackend::do_fast_event(std::vector<EVIO *> &fast_events)
{
    for (auto w : fast_events)
    {
        // 主线程发出io请求后，后续逻辑可能删掉了这个对象
        if (!w) continue;

        int32_t events = 0;
        {
            std::lock_guard<SpinLock> lg(_ev->fast_lock());
            events           = w->_b_fevents;
            w->_b_fevents    = 0;
            w->_b_fevent_index = 0;
        }

        assert(events);

        if (events & EV_ACCEPT)
        {
            // 初始化新socket，只有ssl用到
            w->_io->do_init_accept();
        }
        else if (events & EV_CONNECT)
        {
            // 初始化新socket，只有ssl用到
            w->_io->do_init_connect();
        }

        // 处理数据发送
        if (events & EV_WRITE)
        {
            auto status = w->send();
            do_io_status(w, EV_WRITE, status);
        }
    }
}

void FinalBackend::do_event(int32_t ev_count)
{
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
        if (!w)
        {
            // 假如一个socket调用io_start，接着调用io_stop，那么它的io_watcher就
            // 不会设置。这里做一下冗余检测
            if (try_del_epoll_fd(fd))
            {
                ELOG("%s:epoll fd no io watcher found", __FUNCTION__);
            }
            continue;
        }

        int32_t events          = 0;             // 最终需要触发的事件
        // 期望触发回调的事件，关闭和错误事件不管用户是否设置，都会触发
        int32_t expect_ev       = w->_b_uevents | EV_ERROR | EV_CLOSE;
        const int32_t kernel_ev = w->_b_kevents; // 内核事件
        if (ev->events & EPOLLOUT)
        {
            if (kernel_ev & EV_WRITE)
            {
                events |= EV_WRITE;
                auto status = w->send();
                do_io_status(w, EV_WRITE, status);
            }

            // 这些事件是一次性的，如果触发了就删除。否则主线程来不及处理会
            // 导致io线程一直触发这个事件
            if (EXPECT_FALSE(kernel_ev & EV_CONNECT))
            {
                w->_b_uevents &= static_cast<uint8_t>(~EV_CONNECT);
                modify_watcher(w);
            }

            events |= (EV_WRITE | EV_CONNECT);
        }
        if (ev->events & EPOLLIN)
        {
            if (kernel_ev & EV_READ)
            {
                events |= EV_READ;

                auto status = w->recv();
                do_io_status(w, EV_READ, status);
            }

            // 这些事件是一次性的，如果触发了就删除。否则主线程来不及处理会
            // 导致io线程一直触发这个事件
            if (EXPECT_FALSE(kernel_ev & EV_ACCEPT))
            {
                w->_b_uevents &= static_cast<uint8_t>(~EV_ACCEPT);
                modify_watcher(w);
            }

            events |= (EV_READ | EV_ACCEPT);
        }
        // EPOLLERR | EPOLLHUP和EPOLLOUT | EPOLLIN是同时存在的
        // 但有时候watcher并没有设置EV_WRITE或者EV_READ，因此需要独立检测
        if (ev->events & (EPOLLERR | EPOLLHUP))
        {
            events |= EV_CLOSE;
        }

        // 只触发用户希望回调的事件，例如events有EV_WRITE和EV_CONNECT
        // 用户只设置EV_CONNECT那就不要触发EV_WRITE
        // 假如socket关闭时，用户已不希望回调任何事件，但这时对方可能会发送数据
        // 触发EV_READ事件
        expect_ev &= events;
        if (expect_ev) feed_receive_event(w, expect_ev);
    }
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

    if (0 != modify_fd(_wake_fd, EPOLL_CTL_ADD, EV_READ))
    {
        FATAL("backend init wake_fd fail:%s", strerror(errno));
        return;
    }

    /// 第一次进入wait前，必须先设置io事件，不然的话第一次就会wait很久
    {
        std::lock_guard<std::mutex> lg(_ev->lock());
        do_modify();
    }

    std::vector<EVIO *> fast_events;
    fast_events.reserve(1024);

    int64_t last = get_time_interval();

    // 不能wait太久，要定时检测超时删除的连接
    static const int32_t wait_tm = 2000;

    while (!_done)
    {
        int32_t ev_count = epoll_wait(_ep_fd, _ep_ev, EPOLL_MAXEV, wait_tm);
        if (EXPECT_FALSE(ev_count < 0))
        {
            if (errno != EINTR)
            {
                FATAL("epoll_wait errno(%d)", errno);
            }

            _ev->quit();
            break;
        }

        _has_ev = false;

        // 主线程和io线程会频繁交换action,因此使用一个快速、独立的锁
        {
            fast_events.clear();
            std::lock_guard<SpinLock> lg(_ev->fast_lock());
            fast_events.swap(_ev->get_fast_event());
        }

        // TODO 下面的操作，是每个操作，加锁、解锁一次，还是全程解锁呢？
        // 即使全程加锁，至少是不会影响主线程执行逻辑的。主线程执行逻辑回调时，不会用到锁
        {
            std::lock_guard<std::mutex> lg(_ev->lock());

            do_fast_event(fast_events);
            do_event(ev_count);
            do_modify();

            // 检测待删除的连接是否超时
            int64_t now = get_time_interval();
            if (now - last > wait_tm)
            {
                last = now;
                check_pending_watcher(now);
            }

            if (_has_ev)
            {
                _ev->set_job(true);
                _ev->wake();
            }
        }

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


    w->_b_uevents = w->_uevents;

    _modify_fd.push_back(fd);
}

void FinalBackend::do_modify()
{
    for (auto &fd : _modify_fd)
    {
        EVIO *w = _ev->get_fast_io(fd);

        assert(w);
        modify_watcher(w);
    }
    _modify_fd.clear();
}

int32_t FinalBackend::modify_watcher(EVIO *w)
{
    // 对方已经关闭了连接
    if (w->_b_eevents & EV_CLOSE) return 0;

    int32_t new_ev = 0;
    int32_t op = EPOLL_CTL_DEL;

    auto b_uevents = w->_b_uevents;
    if (b_uevents & EV_FLUSH)
    {
        // 尝试发送一下，大部分情况下都是没数据可以发送的
        // 如果有，那也应该会在fast_event那边处理
        if (IO::IOS_WRITE == w->send())
        {
            int32_t old_ev = w->_b_kevents;
            new_ev = w->_b_eevents;

            op = old_ev ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

            add_pending_watcher(w->_fd);
        }
        else
        {
            // 没有数据要发送或者发送出错直接关闭连接，并通知主线程那边做后续清理工作
            feed_receive_event(w, EV_CLOSE);
        }
    }
    else if (b_uevents & EV_CLOSE)
    {
        // 直接关闭连接，并通知主线程那边做后续清理工作
        feed_receive_event(w, EV_CLOSE);
    }
    else
    {
        int32_t old_ev = w->_b_kevents;
        new_ev = b_uevents | w->_b_eevents;

        op = old_ev ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    }

    // 关闭连接时，这里必须置0，这样do_event才不会继续执行读写逻辑
    w->_b_kevents = static_cast<uint8_t>(new_ev);

    return modify_fd(w->_fd, op, new_ev);
}

bool FinalBackend::try_del_epoll_fd(int32_t fd)
{
    struct epoll_event ev;
    /* valgrind: uninitialised byte(s) */
    memset(&ev, 0, sizeof(ev));

    ev.data.fd = fd;
    ev.events  = 0;

    if (epoll_ctl(_ep_fd, EPOLL_CTL_MOD, fd, &ev)) return true;

    if (ENOENT == errno) return false;

    ELOG("%s error: %d", __FUNCTION__, errno);
    return false;
}

int32_t FinalBackend::modify_fd(int32_t fd, int32_t op, int32_t new_ev)
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

    if (EXPECT_TRUE(!epoll_ctl(_ep_fd, op, fd, &ev))) return 0;

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

void FinalBackend::add_pending_watcher(int32_t fd)
{
    int64_t now = get_time_interval();
    auto ret = _pending_watcher.emplace(fd, now);
    if (false == ret.second)
    {
        ELOG("pending watcher already exist: %d", fd);
    }
}

void FinalBackend::check_pending_watcher(int64_t now)
{
    // C++14以后，允许在循环中删除
    static_assert(__cplusplus > 201402L);

    for (auto iter = _pending_watcher.begin(); iter != _pending_watcher.end(); iter++)
    {
        if (now - iter->second > 10000)
        {
            del_pending_watcher(iter->first, nullptr);
            iter = _pending_watcher.erase(iter);
        }
    }
}

void FinalBackend::del_pending_watcher(int32_t fd, EVIO *w)
{
    // 未传w则是定时检测时调用，在for循环中已经删除了，这里不需要再次删除
    if (!w)
    {
        w = _ev->get_fast_io(fd);
        assert(w && fd == w->_fd);
    }
    else
    {
        _pending_watcher.erase(fd);
    }

    // 标记为已关闭
    w->_b_eevents |= EV_CLOSE;

    feed_receive_event(w, EV_CLOSE);
    modify_fd(w->_fd, EPOLL_CTL_DEL, 0);
}
