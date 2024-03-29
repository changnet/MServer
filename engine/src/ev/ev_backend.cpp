#include "ev.hpp"
#include "ev_backend.hpp"

#if defined(__linux__)
    #include "ev_epoll.inl"
#elif defined(__windows__)
    #include "ev_poll.inl"
#endif


EVBackend *EVBackend::instance()
{
    return new FinalBackend();
}

void EVBackend::uninstance(EVBackend *backend)
{
    delete backend;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

EVBackend::EVBackend()
{
    _has_ev = false;
    _busy = false;
    _done = false;
    _ev   = nullptr;
    _last_pending_tm = 0;
    _modify_protected = false;
    _fast_events.reserve(1024);
}

EVBackend::~EVBackend()
{
}

bool EVBackend::start(class EV *ev)
{
    _ev = ev;

    if (!before_start()) return false;

    _thread = std::thread(&EVBackend::backend, this);

    return true;
}

void EVBackend::stop()
{
    _done = true;
    wake();
    _thread.join();

    after_stop();
}

void EVBackend::backend_once(int32_t ev_count, int64_t now)
{
    _fast_events.clear();

    // TODO 下面的操作，是每个操作，加锁、解锁一次，还是全程解锁呢？
    // 即使全程加锁，至少是不会影响主线程执行逻辑的。主线程执行逻辑回调时，不会用到锁
    {
        std::lock_guard<std::mutex> guard(_ev->lock());

        // poll等结构在处理事件时需要for循环遍历所有fd列表
        // 中间禁止调用modify_fd来删除这个列表
        // epoll则是可以删除的
        _modify_protected = true;
        do_wait_event(ev_count);
        _modify_protected = false;

        // 主线程和backend线程会频繁交换fast_event,因此使用一个快速、独立的锁
        // 注意这里必须在do_wait_event之后交换数据，因为唤醒backend的eventfd在wait_event
        // 必须先清空eventfd再交换数据，否则可能会漏掉一些事件
        {
            std::lock_guard<SpinLock> guard(_ev->fast_lock());
            _fast_events.swap(_ev->get_fast_event());
        }

        do_fast_event();
        do_user_event();

        // 检测待删除的连接是否超时
        static const int32_t MIN_PENDING_CHECK = 2000;
        if (now - _last_pending_tm > MIN_PENDING_CHECK)
        {
            _last_pending_tm = now;
            check_pending_watcher(now);
        }

        if (_has_ev)
        {
            _ev->set_job(true);
            _ev->wake(false);
        }
    }
}

void EVBackend::backend()
{
    int64_t last = EV::steady_clock();

    // 第一次进入wait前，可能主线程那边已经有新的io需要处理
    backend_once(0, last);

    // 不能wait太久，要定时检测超时删除的连接
    static const int32_t max_wait = 2000;

    while (!_done)
    {
        _has_ev = false;

        int32_t ev_count = wait(max_wait);
        if (ev_count < 0) break;

        int64_t now = EV::steady_clock();

        // 对于实时性要求不高的，适当降低backend运行的帧数可以让io读写效率更高
        // 使用#define，当数值为0时直接不编译这部分代码
        #define min_wait 0
#if min_wait
        int64_t diff = min_wait - (now - last);
        if (diff > 1)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(diff));
        }
#endif

        last = now; // 判断是否sleep必须包含backend执行逻辑的时间
        backend_once(ev_count, now);
    }
}

void EVBackend::modify_later(EVIO *w, int32_t events)
{
    w->_b_uevents = static_cast<uint8_t>(events);
    if (!w->_b_uevent_index)
    {
        _user_events.push_back(w);
        w->_b_uevent_index = (int32_t)_user_events.size();
    }
}

void EVBackend::modify(EVIO *w)
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

    modify_later(w, w->_uevents);
}

void EVBackend::do_user_event()
{
    for (auto w : _user_events)
    {
        // 冗余校验是否被主线程删除
        assert(get_fd_watcher(w->_fd));

        w->_b_uevent_index = 0;
        modify_watcher(w);
    }
    _user_events.clear();
}

int32_t EVBackend::modify_watcher(EVIO *w)
{
    // 对方已经关闭了连接，无需再次删除或者修改
    if (w->_b_eevents & EV_CLOSE) return 0;

    int32_t new_ev = 0;
    int32_t op = FD_OP_DEL;

    auto b_uevents = w->_b_uevents;
    if (b_uevents & EV_FLUSH)
    {
        // 尝试发送一下，大部分情况下都是没数据可以发送的
        // 如果有，那也应该会在fast_event那边处理
        if (IO::IOS_WRITE == w->send())
        {
            int32_t old_ev = w->_b_kevents;
            new_ev = w->_b_eevents;

            op = old_ev ? FD_OP_MOD : FD_OP_ADD;

            add_pending_watcher(w->_fd);
        }
        else
        {
            // 没有数据要发送或者发送出错直接关闭连接，并通知主线程那边做后续清理工作
            // 这个modify可能是backend本身发起的，仍要检测是否要从pending中删除
            del_pending_watcher(w->_fd, w);
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

        op = old_ev ? FD_OP_MOD : FD_OP_ADD;
    }

    // 关闭连接时，这里必须置0，这样do_event才不会继续执行读写逻辑
    w->_b_kevents = static_cast<uint8_t>(new_ev);

    return modify_fd(w->_fd, op, new_ev);
}

void EVBackend::feed_receive_event(EVIO *w, int32_t ev)
{
    _has_ev = true;
    _ev->io_receive_event(w, ev);
}


bool EVBackend::do_io_status(EVIO *w, int32_t ev, const IO::IOStatus &status)
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
                modify_later(w, EV_CLOSE);
            }
            else if (w->_b_eevents & EV_WRITE)
            {
                w->_b_eevents &= static_cast<uint8_t>(~EV_WRITE);
                modify_later(w, w->_b_uevents);
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
            modify_later(w, w->_b_uevents);
        }
        return true;
    case IO::IOS_BUSY:
        // 正常情况不出会出缓冲区溢出的情况，客户端之间可直接kill
        if (w->_mask & EVIO::M_OVERFLOW_KILL)
        {
            PLOG("backend thread fd overflow kill id = %d, fd = %d", w->_id,
                 w->_fd);
            modify_later(w, EV_CLOSE);
            return false;
        }
        else
        {
            // 读溢出的话，稍微sleep一下不至于占用太多cpu即可，其他没什么影响
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            ELOG("backend thread fd overflow sleep");
        }
        return true;
    case IO::IOS_CLOSE:
    case IO::IOS_ERROR:
        modify_later(w, EV_CLOSE);
        return false;
    default: ELOG("unknow io status: %d", status); return false;
    }

    return false;
}

void EVBackend::do_watcher_fast_event(EVIO *w)
{
    int32_t events = 0;
    {
        std::lock_guard<SpinLock> guard(_ev->fast_lock());

        // 执行fast_event时，需要保证watcher已就位
        // 因为fast_event可能会修改该watcher在epoll中的信息从而产生event
        // 这时候没有watcher就无法正确处理
        assert(get_fd_watcher(w->_fd));

        events           = w->_b_fevents;
        w->_b_fevents    = 0;
        w->_b_fevent_index = 0;
    }

    // TODO 执行fast_event时，可能还未执行该watcher的user_event
    // 即当前fd仍不归epoll管理从目前来看这个暂时没有影响
    // 例如accept成功时直接发送数据，fast_event直接唤醒线程发送数据，但do_user_event
    // 要在发送完后才执行

    assert(events);

    if (events & EV_ACCEPT)
    {
        // 初始化新socket，只有ssl用到
        auto status = w->do_init_accept();
        do_io_status(w, EV_READ, status);
    }
    else if (events & EV_CONNECT)
    {
        // 初始化新socket，只有ssl用到
        auto status = w->do_init_connect();
        do_io_status(w, EV_WRITE, status);
    }

    // 处理数据发送
    if (events & EV_WRITE)
    {
        auto status = w->send();
        do_io_status(w, EV_WRITE, status);
    }
}

void EVBackend::do_watcher_wait_event(EVIO *w, int32_t revents)
{
    int32_t events          = 0;             // 最终需要触发的事件
    // 期望触发回调的事件，关闭和错误事件不管用户是否设置，都会触发
    int32_t expect_ev       = w->_b_uevents | EV_ERROR | EV_CLOSE;
    const int32_t kernel_ev = w->_b_kevents; // 内核事件
    if (revents & EV_WRITE)
    {
        if (kernel_ev & EV_WRITE)
        {
            events |= EV_WRITE;
            auto status = w->send();
            do_io_status(w, EV_WRITE, status);
        }

        // 这些事件是一次性的，如果触发了就删除。否则导致一直触发这个事件
        if (EXPECT_FALSE(kernel_ev & EV_CONNECT))
        {
            w->_b_uevents &= static_cast<uint8_t>(~EV_CONNECT);
            modify_later(w, w->_b_uevents);
        }

        events |= (EV_WRITE | EV_CONNECT);
    }
    if (revents & EV_READ)
    {
        if (kernel_ev & EV_READ)
        {
            events |= EV_READ;

            auto status = w->recv();
            do_io_status(w, EV_READ, status);
        }

        events |= (EV_READ | EV_ACCEPT);
    }

    if (revents & (EV_ERROR | EV_CLOSE))
    {
        events |= EV_CLOSE;
        // 如果对方关闭，这里直接从backend移除监听，避免再从主线程通知删除
        // 注意需要清除EV_FLUSH标记，那个优先级更高
        modify_later(w, EV_CLOSE);
    }

    // 只触发用户希望回调的事件，例如events有EV_WRITE和EV_CONNECT
    // 用户只设置EV_CONNECT那就不要触发EV_WRITE
    // 假如socket关闭时，用户已不希望回调任何事件，但这时对方可能会发送数据
    // 触发EV_READ事件
    expect_ev &= events;
    if (expect_ev) feed_receive_event(w, expect_ev);
}

void EVBackend::do_fast_event()
{
    for (auto w : _fast_events)
    {
        // 主线程发出io请求后，后续逻辑可能删掉了这个对象
        if (!w) continue;

        do_watcher_fast_event(w);
    }
}

void EVBackend::add_pending_watcher(int32_t fd)
{
    int64_t now = EV::steady_clock();
    auto ret = _pending_watcher.emplace(fd, now);
    if (false == ret.second)
    {
        ELOG("pending watcher already exist: %d", fd);
    }
}

void EVBackend::check_pending_watcher(int64_t now)
{
    // C++14以后，允许在循环中删除
    static_assert(__cplusplus > 201402L);

    for (auto iter = _pending_watcher.begin(); iter != _pending_watcher.end(); iter++)
    {
        // 超时N毫秒后删除
        if (now - iter->second > 10000)
        {
            del_pending_watcher(iter->first, nullptr);
            iter = _pending_watcher.erase(iter);
        }
    }
}

void EVBackend::del_pending_watcher(int32_t fd, EVIO *w)
{
    // 未传w则是定时检测时调用，在for循环中已经删除了，这里不需要再次删除
    if (!w)
    {
        w = get_fd_watcher(fd);

        // 多次调用close可能会删掉watcher
        if (!w)
        {
            ELOG("no watcher found: %d", fd);
            return;
        }
    }
    else
    {
        _pending_watcher.erase(fd);
    }

    // 标记为已关闭
    w->_b_eevents |= EV_CLOSE;

    feed_receive_event(w, EV_CLOSE);
    modify_fd(w->_fd, FD_OP_DEL, 0);
}

void EVBackend::set_fd_watcher(int32_t fd, EVIO *w)
{
    // win的socket是unsigned类型，可能会很大，得强转unsigned来判断
    uint32_t ufd = ((uint32_t)fd);
    if (ufd < HUGE_FD)
    {
        if (EXPECT_FALSE(_fd_watcher.size() < ufd))
        {
            _fd_watcher.resize(ufd + 1024);
        }
        _fd_watcher[fd] = w;
    }
    else
    {
        if (w)
        {
            _fd_watcher_huge[fd] = w;
        }
        else
        {
            _fd_watcher_huge.erase(fd);
        }
    }
}

EVIO *EVBackend::get_fd_watcher(int32_t fd)
{
    uint32_t ufd = ((uint32_t)fd);
    if (ufd < HUGE_FD) return _fd_watcher[ufd];

    auto found = _fd_watcher_huge.find(fd);
    return found == _fd_watcher_huge.end() ? nullptr : found->second;
}
