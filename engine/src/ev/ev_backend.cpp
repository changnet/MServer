#include "ev_backend.hpp"
#include "thread/thread.hpp"
#include "poll_backend.hpp"
#include "epoll_backend.hpp"
#include "system/static_global.hpp"

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
    done_ = false;
    busy_             = false;
    modify_protected_ = false;
}

EVBackend::~EVBackend()
{
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

    do_recv_events();
    do_pending_events();
}

void EVBackend::backend()
{
    Thread::apply_thread_name("ev_backend");
    int64_t last = StaticGlobal::E->steady_clock();

    // 第一次进入wait前，可能主线程那边已经有新的io需要处理
    backend_once(0, last);

    static const int32_t min_wait = 1; // 不为0
    static const int32_t max_wait = 2000;

    while (!done_)
    {
        int32_t ev_count = wait(busy_ ? min_wait : max_wait);
        if (ev_count < 0) break;

        int64_t now = StaticGlobal::E->steady_clock();

        // 对于实时性要求不高的，适当降低backend运行的帧数可以让io读写效率更高
        // 使用#define，当数值为0时直接不编译这部分代码
        #define min_wait 0
#if min_wait
        int64_t diff = min_wait - (now - last);
        if (diff > 1)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(diff));
        }
        now = StaticGlobal::E->steady_clock();
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
    for (const auto& w : pending_events_)
    {
        int32_t events = w->b_pevents_;
        w->b_pevents_ = 0;
        modify_watcher(w, events);
    }
    pending_events_.clear();
}

int32_t EVBackend::modify_watcher(EVIO *w, int32_t events)
{
    int32_t op = 0;

    // EV_FLUSH来自主线程，
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
        op = FD_OP_DEL;
        if (fd_mgr_.unset(w))
        {
            dispatch_event(w, EV_CLOSE);
        } 
    }
    else
    {
        op = (w->b_kevents_ & EV_KERNEL) ? FD_OP_MOD : FD_OP_ADD;
        if (w->b_kevents_ & EV_KERNEL)
        {
            op = FD_OP_MOD;
            assert(0 != events);
        }
        else
        {
            op = FD_OP_ADD;
            assert(!fd_mgr_.get(w->fd_));
            assert(0 == w->b_kevents_ && 0 != events);

            if (!fd_mgr_.set(w)) return 0;
        }

        // 即使是FD_OP_MOD，也要重新设置EV_KERNEL，因为events里不包含EV_KERNEL
        events |= EV_KERNEL;
        w->b_kevents_ = events;
    }

    // 注意：到这里，w可能已经被delete
    return modify_fd(fd, op, events);
}

bool EVBackend::do_io_status(EVIO *w, int32_t ev, const int32_t &status)
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
                modify_later(w, EV_CLOSE);
                ELOG_R("flush write close %d", w->fd_);
            }
            else if (w->b_kevents_ & EV_WRITE)
            {
                // 发送完成，从epoll中取消EV_WRITE事件
                modify_later(w, w->b_kevents_ & (~EV_WRITE));
            }
        }
        return true;
    case EV_READ:
        // socket一般都会监听读事件，不需要做特殊处理
        return true;
    case EV_WRITE:
        // 未发送完，加入write事件继续发送
        if (!(w->b_kevents_ & EV_WRITE))
        {
            modify_later(w, w->b_kevents_ | EV_WRITE);
        }
        return true;
    case EV_BUSY:
        // 正常情况不出会出缓冲区溢出的情况，客户端之间可直接kill
        if (w->mask_ & EVIO::M_OVERFLOW_KILL)
        {
            PLOG("backend thread overflow kill fd = %d", w->fd_);
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
    case EV_CLOSE:
        modify_later(w, EV_CLOSE);
        return false;
    case EV_INIT_ACPT:
    case EV_INIT_CONN:
        dispatch_event(w, status);

        // ssl握手未完成，不应该有数据要发送。ssl中途重新协商？？？ Renegotiation is removed from TLS 1.3
        //int32_t new_status = w->send();
        //do_io_status(w, EV_WRITE, new_status);
        return true;
    case EV_ERROR:
        // w->errno_ = netcompat::errorno(); // 这个已经在send、recv里设置
        modify_later(w, EV_CLOSE);
        return false;
    default: ELOG("unknow io status: %d", status); return false;
    }

    return false;
}

void EVBackend::do_watcher_recv_event(EVIO *w, int32_t events)
{
    assert(events);

    // epoll检测到连接已断开，会直接从backend中删除
    // 此时再收到主线程的任何数据都直接丢弃
    if (w->b_kevents_ & EV_CLOSE || w->b_pevents_ & EV_CLOSE) return;

    if (events & EV_INIT_ACPT)
    {
        // 初始化新socket，只有ssl用到
        int32_t status = w->do_init_accept();
        do_io_status(w, EV_INIT_ACPT, status);
    }
    else if (events & EV_INIT_CONN)
    {
        // 初始化新socket，只有ssl用到
        int32_t status = w->do_init_connect();
        do_io_status(w, EV_INIT_CONN, status);
    }

    // 处理数据发送
    if (events & EV_WRITE)
    {
        auto status = w->send();
        do_io_status(w, EV_WRITE, status);
    }

    // 其他事件，和从poll、epoll的事件汇总后，再一起处理
    static const int32_t EV = EV_READ | EV_ACCEPT | EV_CONNECT | EV_CLOSE | EV_FLUSH;

    events &= EV;
    if (events)
    {
        modify_later(w, events);
    }
}

void EVBackend::do_watcher_wait_event(EVIO *w, int32_t revents)
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
    int32_t events          = 0;             // 最终需要触发的事件
    const int32_t kernel_ev = w->b_kevents_; // 内核事件
    if (revents & EV_WRITE)
    {
        if (kernel_ev & EV_WRITE)
        {
            auto status = w->send();
            do_io_status(w, EV_WRITE, status);
        }

        // 这些事件是一次性的，如果触发了就删除。否则导致一直触发这个事件
        if (unlikely(kernel_ev & EV_CONNECT))
        {
            modify_later(w, kernel_ev & (~EV_CONNECT));
        }

        events |= (EV_WRITE | EV_CONNECT);
    }
    if (revents & EV_READ)
    {
        if (kernel_ev & EV_READ)
        {
            auto status = w->recv();
            do_io_status(w, EV_READ, status);
        }
        if (kernel_ev & EV_ACCEPT)
        {
            auto status = w->io_->accept(w);
            do_io_status(w, EV_ACCEPT, status);
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
    // 假如socket关闭时，用户已不希望回调任何事件，但这时对方可能会发送数据,触发EV_READ事件
    // 关闭和错误事件不管用户是否设置，都会触发
    int32_t expect_ev = kernel_ev | EV_ERROR | EV_CLOSE;
    expect_ev &= events;
    if (expect_ev) dispatch_event(w, expect_ev);
}

void EVBackend::do_recv_events()
{
    // 只循环一定次数，避免其他线程不断地发送事件，导致线程一直在处理这些事件而
    // 不再接收来自网络的事件
    EVIO *w;
    for (int32_t i = 1; i < 1024; i++)
    {
        {
            std::scoped_lock<std::mutex> sl(mutex_);
            if (recv_events_.empty())
            {
                busy_ = false;
                return;
            }

            w = recv_events_.front();
            recv_events_.pop_front();
        }

        int32_t ev = w->b_ev_.exchange(0);
        do_watcher_recv_event(w, ev);
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

void EVBackend::append_event(EVIO *w, int32_t ev)
{
    int32_t old = w->b_ev_.fetch_or(ev);
    if (0 != old) return;

    {
        std::scoped_lock<std::mutex> sl(mutex_);
        recv_events_.push_back(w);
    }
    wake();
}