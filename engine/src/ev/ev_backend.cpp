#include "ev_backend.hpp"
#include "thread/thread.hpp"
#include "poll_backend.hpp"
#include "epoll_backend.hpp"
#include "system/static_global.hpp"
#include "time.hpp"

#ifdef __windows__
    #include <ws2tcpip.h>
#else
    #include <fcntl.h>
    #include <sys/types.h>
    #include <sys/socket.h>
#endif

EVBackend *EVBackend::instance()
{
#ifdef __poll__
    return new PollBackend;
#else
    return new EpollBackend();
#endif
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

EVBackend::EVBackend()
{
    done_             = false;
    busy_             = false;
    modify_protected_ = false;
}

EVBackend::~EVBackend()
{
    fd_mgr_.clear();
}

bool EVBackend::start()
{
    if (!before_start()) return false;

    thread_ = std::thread(&EVBackend::backend, this);

    return true;
}

void EVBackend::stop()
{
    done_ = true;
    wake();
    thread_.join();

    after_stop();
}

void EVBackend::backend_once(int32_t ev_count, int64_t now)
{
    // poll等结构在处理事件时需要for循环遍历所有fd列表
    // 中间禁止调用modify_fd来删除这个列表
    // epoll则是可以删除的
    modify_protected_ = true;
    do_wait_event(ev_count);
    modify_protected_ = false;

    do_watcher_events();
    do_pending_events();
}

void EVBackend::backend()
{
    Thread::apply_thread_name("ev_backend");
    int64_t last = timing::steady_clock();

    // 第一次进入wait前，可能主线程那边已经有新的io需要处理
    backend_once(0, last);

    static const int32_t min_wait = 1; // 不为0
    static const int32_t max_wait = 2000;

    while (!done_)
    {
        int32_t ev_count = wait(busy_ ? min_wait : max_wait);
        if (ev_count < 0) break;

        int64_t now = timing::steady_clock();

// 对于实时性要求不高的，适当降低backend运行的帧数可以让io读写效率更高
// 使用#define，当数值为0时直接不编译这部分代码
#define min_wait 0
#if min_wait
        int64_t diff = min_wait - (now - last);
        if (diff > 1)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(diff));
        }
        now = timing::steady_clock();
#endif

        last = now; // 判断是否sleep必须包含backend执行逻辑的时间
        backend_once(ev_count, now);
    }
}

void EVBackend::modify_later(EVIO *w, int32_t events)
{
    // poll收到读写事件时（比如连接断开），是不能修改poll数组的
    // 因此只能先把事件暂存起来，稍后统一处理

    if (!w->b_pevents_) pending_events_.push_back(w);

    w->b_pevents_ |= events;
}

void EVBackend::do_pending_events()
{
    for (const auto &w : pending_events_)
    {
        int32_t events = w->b_pevents_;
        w->b_pevents_  = 0;
        modify_watcher(w, events);
    }
    pending_events_.clear();
}

int32_t EVBackend::modify_watcher(EVIO *w, int32_t events)
{
    int32_t op = 0;

    // EV_CLOSE来自当前线程表示socket已经关闭，即使有EV_FLUSH标识也没法
    // EV_FLUSH来逻辑线程，逻辑线程不会同时发 EV_FLUSH|EV_CLOSE
    if (events & EV_FLUSH && !(events & EV_CLOSE))
    {
        // 尝试发送一下，大部分情况下都是没数据可以发送的，或者一次就可以发送完成
        if (EV_WRITE == w->send())
        {
            events |= EV_WRITE; // 继续发送
        }
        else
        {
            events |= EV_CLOSE; // 发送完成，直接关闭
        }
    }

    int32_t fd = w->fd_;
    if (events & EV_CLOSE)
    {
        /**
         * 当对方关闭链接时，backend线程会从fd_mgr移除并向逻辑线程发送关闭消息
         * 逻辑线程在收到消息前也恰好关闭连接，也会向backend线程发送关闭消息
         *
         * 所以backend线程收到关闭消息时发现已经关闭，则无需处理
         *
         * 注意：backend线程从不关闭fd，所以不存在fd复用的问题
         */
        if (!fd_mgr_.get(w->fd_)) return 0;

        op = FD_OP_DEL;
        if (fd_mgr_.unset(w))
        {
            dispatch_event(w, EV_CLOSE);
        }
        w->b_kevents_ &= (~EV_WRITE);
    }
    else
    {
        if (w->b_kevents_ & EV_KERNEL)
        {
            op = FD_OP_MOD;
            // events可能是0。在连接过程中，连接收到返回，还来不及设置数据到backend
            // assert(0 != events);
        }
        else
        {
            assert(!fd_mgr_.get(w->fd_));
            assert(0 == w->b_kevents_ && 0 != events);

            op = FD_OP_ADD;
            if (!fd_mgr_.set(w)) return 0;
        }

        // 即使是FD_OP_MOD，也要重新设置EV_KERNEL，因为events里不包含EV_KERNEL
        w->b_kevents_ = events | EV_KERNEL;
    }

    // 注意：到这里，w可能已经被delete
    return modify_fd(fd, op, events);
}

void EVBackend::do_io_status(EVIO *w, int32_t ev, int32_t status,
                             int32_t &events, int32_t &kevents)
{
    switch (status)
    {
    case EV_NONE:
        // 发送完则需要删除写事件，不然会一直触发
        if (EV_WRITE == ev)
        {
            // 发送完后主动关闭主动关闭
            if (w->b_kevents_ & EV_FLUSH)
            {
                kevents |= EV_CLOSE;
            }
            else if (w->b_kevents_ & EV_WRITE)
            {
                // 发送完成，从epoll中取消EV_WRITE事件
                kevents &= ~EV_WRITE;
            }
        }
        break;
    case EV_READ:
        // ssl握手需要继续读取数据时，需要添加可读事件
        kevents |= EV_READ;
        break;
    case EV_WRITE:
        kevents |= EV_WRITE;
        break;
    case EV_BUSY:
        // 正常情况不出会出缓冲区溢出的情况，客户端之间可直接kill
        if (w->mask_ & EVIO::M_OVERFLOW_KILL)
        {
            PLOG("backend thread overflow kill fd = %d", w->fd_);

            kevents |= EV_CLOSE;
        }
        else
        {
            // 读溢出的话，稍微sleep一下不至于占用太多cpu即可，其他没什么影响
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            ELOG("backend thread fd overflow sleep");
        }
        break;
    case EV_CLOSE:
        kevents |= EV_CLOSE;
        break;
    case EV_INIT_ACPT:
    case EV_INIT_CONN:
        if (0 == (status & (EV_ERROR | EV_CLOSE)))
        {
            events |= status;
            // 握手完成之前，读写事件都是无效的。握手完成后重新设置(或者在这里统一设置读事件？)。
            // 因为现在没有区分是因为握手需要读写，还是上层逻辑设置的读写
            kevents &= ~(status | EV_READ | EV_WRITE);
            // ssl握手未完成，不应该有数据要发送。ssl中途重新协商？？？
            // Renegotiation is removed from TLS 1.3
            // int32_t new_status = w->send();
            // do_io_status(w, EV_WRITE, new_status);
        }
        else
        {
            kevents |= EV_CLOSE;
        }
        break;
    case EV_ERROR:
        // w->errno_ = netcompat::errorno(); // 这个已经在send、recv里设置
        kevents |= EV_CLOSE;
        break;
    default: ELOG("unknow io status: %d", status);
    }
}

void EVBackend::do_watcher_event(EVIO *w, int32_t revents, bool add)
{
    assert(revents);

    // epoll检测到连接已断开，会直接从backend中删除
    // 此时再收到主线程的任何数据都直接丢弃
    int32_t b_kevents = w->b_kevents_;
    if (b_kevents & (EV_CLOSE | EV_CLOSE)) return;

    int32_t events = 0; // 需要派发到其他线程的事件
    int32_t kevents =
        add ? (revents | b_kevents) : revents; // 需要设置到kernel的事件

    // 这几个事件，是要立即执行
    if (revents & EV_INIT_ACPT)
    {
        // 初始化新socket，只有ssl用到
        int32_t status = w->do_init_accept();
        do_io_status(w, EV_INIT_ACPT, status, events, kevents);
    }
    else if (revents & EV_INIT_CONN)
    {
        // 初始化新socket，只有ssl用到
        int32_t status = w->do_init_connect();
        do_io_status(w, EV_INIT_CONN, status, events, kevents);
    }

    // 处理数据发送
    if (revents & EV_WRITE)
    {
        auto status = w->send();
        do_io_status(w, EV_WRITE, status, events, kevents);
    }

    // 其他事件，和从poll、epoll的事件汇总后，再一起处理，设置到epoll
    static const int32_t EV = EV_READ | EV_ACCEPT | EV_CONNECT | EV_INIT_ACPT
                              | EV_INIT_CONN | EV_CLOSE | EV_FLUSH;

    kevents &= EV;
    if (kevents != b_kevents) modify_later(w, kevents);

    if (events)
    {
        assert(!(events & EV_CLOSE));
        dispatch_event(w, events);
    }
}

void EVBackend::do_kernel_event(EVIO *w, int32_t revents)
{
    /**
     * 以前单线程的时候，事件可以先记录而不处理，等待回调处理
     * 但是事件放到独立的线程后，如果只把事件发给另一个线程而不处理，那当前线程
     * 进入epoll的wait时，由于另一个线程来不及处理，会直接再次触发事件回调，导致
     * epoll线程消耗大量的cpu
     *
     * 对于读写，数据可以在epoll线程读出来放缓冲区。对于accept，需要先把fd取出来
     * 存到某个地方，等待另一个线程处理
     */

    const int32_t b_kevents = w->b_kevents_; // 内核事件

    int32_t events          = 0;             // 最终需要派发到其他线程的事件
    int32_t kevents         = b_kevents;

    if (revents & EV_ERROR)
    {
        // 出错时，不可能再进行读写操作
        kevents |= EV_ERROR | EV_CLOSE;

        int32_t err   = 0;
        socklen_t len = sizeof(err);
        getsockopt(w->fd_, SOL_SOCKET, SO_ERROR, (char *)&err, &len);
        w->errno_ = err;
    }
    else
    {
        if (revents & EV_CLOSE)
        {
            // 正常关闭，无错误码，但可能有读事件，不可能有写事件
            /**
             * shutdown(fd, SHUT_RD) 关闭本地读功能，对方不可感知。但如果对方发数据会触发RST错误
             * shutdown(fd, SHUT_WR) 触发EPOLLIN和EPOLLHUP，调用recv可能收到数据，也返回0
             */
            kevents |= EV_CLOSE;
        }
        else if (revents & EV_WRITE)
        {
            if (b_kevents & EV_WRITE)
            {
                events |= EV_WRITE;
                auto status = w->send();
                do_io_status(w, EV_WRITE, status, events, kevents);
            }
            else if (unlikely(b_kevents & EV_CONNECT))
            {
                events |= EV_CONNECT;
                kevents &= ~EV_CONNECT;
            }
            else if (unlikely(b_kevents & EV_INIT_ACPT))
            {
                auto status = w->io_->handshake(w);
                do_io_status(w, EV_INIT_ACPT, status, events, kevents);
            }
            else if (unlikely(b_kevents & EV_INIT_CONN))
            {
                auto status = w->io_->handshake(w);
                do_io_status(w, EV_INIT_CONN, status, events, kevents);
            }
            else
            {
                kevents &= ~EV_WRITE; // 移除，避免cpu满载
                ELOG("unexpect write event, id = %d, fd = %d", w->id_, w->fd_);
            }
        }
        if (revents & EV_READ)
        {
            if (b_kevents & EV_READ)
            {
                events |= EV_READ;
                auto status = w->recv();
                do_io_status(w, EV_READ, status, events, kevents);
            }
            else if (b_kevents & EV_ACCEPT)
            {
                events |= EV_ACCEPT;
                auto status = w->io_->accept(w);
                do_io_status(w, EV_ACCEPT, status, events, kevents);
            }
            else if (unlikely(b_kevents & EV_INIT_ACPT))
            {
                auto status = w->io_->handshake(w);
                do_io_status(w, EV_INIT_ACPT, status, events, kevents);
            }
            else if (unlikely(b_kevents & EV_INIT_CONN))
            {
                auto status = w->io_->handshake(w);
                do_io_status(w, EV_INIT_CONN, status, events, kevents);
            }
            else
            {
                kevents &= ~EV_READ;
                ELOG("unexpect read event, id = %d, fd = %d", w->id_, w->fd_);
            }
        }
    }

    /**
     * 只触发用户希望回调的事件，例如events有EV_WRITE和EV_CONNECT
     * 用户只设置EV_CONNECT那就不要触发EV_WRITE
     * 假如socket关闭时，用户已不希望回调任何事件，但这时对方可能会发送数据,触发EV_READ事件
     *
     * 关闭和错误事件不要在这里dispatch，否则backend线程未从epoll移除，主线程可能就关闭socket了
     * 要等modify_later延迟关闭后，再独立派发一次关闭事件
     */

    int32_t expect_ev = b_kevents & events;
    if (expect_ev)
    {
        assert(!(expect_ev & EV_CLOSE)); // 关闭事件必须在backend关闭socket后才能派发
        dispatch_event(w, expect_ev);
    }
    if (kevents != b_kevents) modify_later(w, kevents);
}

void EVBackend::do_watcher_events()
{
    // 只循环一定次数，避免其他线程不断地发送事件，导致线程一直在处理这些事件而
    // 不再接收来自网络的事件
    for (int32_t i = 1; i < 1024; i++)
    {
        EVIO *w;
        int32_t e;
        {
            std::scoped_lock<std::mutex> sl(mutex_);
            if (watcher_events_.empty())
            {
                busy_ = false;
                return;
            }

            const WatcherEvent &we = watcher_events_.front();

            w = we.w_;
            e = we.e_;
            watcher_events_.pop_front();
        }

        if (e)
        {
            do_watcher_event(w, e, false);
        }
        else
        {
            e = w->b_ev_.exchange(0);
            do_watcher_event(w, e, true);
        }
    }

    busy_ = true;
}

void EVBackend::dispatch_event(EVIO *w, int32_t ev)
{
    int32_t old = w->ev_.fetch_or(ev);
    if (0 != old) return;

    bool ok = StaticGlobal::M->forward_message(
        0, w->addr_, ThreadMessage::SOCKET, nullptr, w->id_);
    if (!ok) ELOG("backend forward_message fail: %d", w->addr_);
}

void EVBackend::add_watcher_event(EVIO *w, int32_t ev)
{
    int32_t old = w->b_ev_.fetch_or(ev);
    if (0 != old) return;

    {
        std::scoped_lock<std::mutex> sl(mutex_);
        watcher_events_.emplace_back(0, w);
    }
    wake();
}

void EVBackend::set_watcher_event(EVIO *w, int32_t ev)
{
    {
        std::scoped_lock<std::mutex> sl(mutex_);
        watcher_events_.emplace_back(ev, w);
    }
    wake();
}
