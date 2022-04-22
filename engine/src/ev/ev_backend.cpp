#include "ev.hpp"
#include "ev_backend.hpp"

#if defined(__linux__)
    // #include "ev_epoll.inl"
    #include "ev_poll.inl"
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

void EVBackend::backend_once(int32_t ev_count)
{
    // 主线程和io线程会频繁交换action,因此使用一个快速、独立的锁
    {
        _fast_events.clear();
        std::lock_guard<SpinLock> lg(_ev->fast_lock());
        _fast_events.swap(_ev->get_fast_event());
    }

    // TODO 下面的操作，是每个操作，加锁、解锁一次，还是全程解锁呢？
    // 即使全程加锁，至少是不会影响主线程执行逻辑的。主线程执行逻辑回调时，不会用到锁
    {
        std::lock_guard<std::mutex> lg(_ev->lock());

        do_fast_event();

        // poll等结构在处理事件时需要for循环遍历所有fd列表
        // 中间禁止调用modify_fd来删除这个列表
        // epoll则是可以删除的
        _modify_protected = true;
        do_wait_event(ev_count);
        _modify_protected = false;

        do_modify();

        // 检测待删除的连接是否超时
        static const int32_t MIN_PENDING_CHECK = 2000;
        int64_t now = EV::steady_clock();
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
    // 第一次进入wait前，可能主线程那边已经有新的io需要处理
    backend_once(0);

    // 不能wait太久，要定时检测超时删除的连接
    static const int32_t wait_tm = 2000;

    while (!_done)
    {
        _has_ev = false;

        int32_t ev_count = wait(wait_tm);
        if (ev_count < 0) break;

        backend_once(ev_count);
    }
}

void EVBackend::modify_later(EVIO *w, int32_t events)
{
    w->_b_uevents = events;
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

void EVBackend::do_modify()
{
    for (auto w : _user_events)
    {
        // 冗余校验是否被主线程删除
        assert(_ev->get_fast_io(w->_fd));

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
        // 主线程忙不过来了，子线程需要sleep一下等待主线程
        _busy = true;
        return true;
    case IO::IOS_CLOSE:
    case IO::IOS_ERROR:
        // TODO 错误时是否要传一个错误码给主线程
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
        std::lock_guard<SpinLock> lg(_ev->fast_lock());
        events           = w->_b_fevents;
        w->_b_fevents    = 0;
        w->_b_fevent_index = 0;
    }

    assert(events);

    if (events & EV_ACCEPT)
    {
        // 初始化新socket，只有ssl用到
        auto status = w->do_init_accept();
        do_io_status(w, EV_WRITE, status);
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

        // 这些事件是一次性的，如果触发了就删除。否则主线程来不及处理会
        // 导致io线程一直触发这个事件
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

        // 这些事件是一次性的，如果触发了就删除。否则主线程来不及处理会
        // 导致io线程一直触发这个事件
        if (EXPECT_FALSE(kernel_ev & EV_ACCEPT))
        {
            w->_b_uevents &= static_cast<uint8_t>(~EV_ACCEPT);
            modify_later(w, w->_b_uevents);
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
        w = _ev->get_fast_io(fd);

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
