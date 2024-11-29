#include "ev_watcher.hpp"

#include "ev.hpp"

EVWatcher::EVWatcher(EV *loop) : loop_(loop)
{
}
////////////////////////////////////////////////////////////////////////////////

EVIO::EVIO(int32_t fd, EV *loop) : EVWatcher(loop)
{
    fd_     = fd;

    mask_ = 0;
    errno_ = 0;

    b_kevents_ = 0;
    b_pevents_ = 0;

    ev_counter_ = 0;
    ev_index_   = 0;

    b_ev_counter_ = 0;
    b_ev_index_   = 0;

    io_ = nullptr;
#ifndef NDEBUG
    ref_ = 0;
#endif
}

EVIO::~EVIO()
{
#ifndef NDEBUG
    assert(0 == ref_);
#endif
    if (io_) delete io_;
}

void EVIO::add_ref(int32_t v)
{
#ifndef NDEBUG
    ref_ += v;
#endif
}

void EVIO::set(int32_t events)
{
    loop_->append_event(this, events);
}

int32_t EVIO::recv()
{
    // lua报错，在启动socket对象后无法正常设置io参数
    if (!io_) return EV_ERROR;

    return io_->recv(this);
}

int32_t EVIO::send()
{
    // lua报错，在启动socket对象后无法正常设置io参数
    if (!io_) return EV_ERROR;

    return io_->send(this);
}

int32_t EVIO::do_init_accept()
{
    // lua报错，在启动socket对象后无法正常设置io参数
    if (!io_) return EV_ERROR;

    return io_->do_init_accept(fd_);
}

int32_t EVIO::do_init_connect()
{
    // lua报错，在启动socket对象后无法正常设置io参数
    if (!io_) return EV_ERROR;

    return io_->do_init_connect(fd_);
}

////////////////////////////////////////////////////////////////////////////////

EVTimer::EVTimer(int32_t id, EV *loop) : EVWatcher(loop)
{
    pending_ = 0;
    revents_ = 0;
    id_     = id;
    index_  = 0;
    policy_ = P_NONE;
    at_     = 0;
    repeat_ = 0;
}

EVTimer::~EVTimer()
{
}

void EVTimer::reschedule(int64_t now)
{
    /**
     * 当前用的是CLOCK_MONOTONIC时间，所以不存在用户调时间的问题
     * 但可能存在卡主循环的情况，libev默认情况下是修正为当前时间
     */
    switch (policy_)
    {
    case P_ALIGN:
    {
        // 严格对齐到特定时间，比如一个定时器每5秒触发一次，那必须是在 0 5 10 15
        // 触发 即使主线程卡了，也不允许在其他秒数触发
        assert(repeat_ > 0);
        while (at_ < now) at_ += repeat_;
        break;
    }
    case P_SPIN:
    {
        // 自旋到时间恢复正常
        // 假如定时器1秒触发1次，现在过了5秒，则会在很短时间内触发5次回调
        break;
    }
    default:
    {
        // 按当前时间重新计时，这是最常用的定时器，libev、libevent都是这种处理方式
        at_ = now;
        break;
    }
    }
}

void EVTimer::callback(int32_t revents)
{
    loop_->timer_callback(id_, revents);
}

int32_t EventSwapList::append_event(EVIO *w, int32_t ev,int32_t &counter, int32_t &index)
{
    std::lock_guard<SpinLock> guard(lock_);

    // 如果当前socket已经在队列中，则不需要额外附加一个event
    // 否则发协议时会导致事件数组很长
    if (counter_ == counter)
    {
        assert(append_.size() > (size_t)index);

        WatcherEvent &fe = append_[index];

        assert(fe.w_ == w);
        fe.ev_ |= ev;

        return 0;
    }
    else
    {
        int32_t flag = 1;
        size_t size  = append_.size();

        // 使用counter来判断socket是否已经在数组中，引出的额外问题是counter重置时
        // 需要遍历所有socket来重置对应的counter
        if (0x7FFFFFFF == counter_)
        {
            counter_ = 1;
            flag     = 2;
        }
        else
        {
            flag = size > 0 ? 0 : 1;
        }
        counter = counter_;
        index   = int32_t(size); // 从0开始，不是size + 1
        append_.emplace_back(w, ev);

        return flag;
    }
}
