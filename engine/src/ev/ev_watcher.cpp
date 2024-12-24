#include "ev_watcher.hpp"

EVIO::EVIO(int32_t id, int32_t addr, int32_t fd)
{
    fd_   = fd;
    id_   = id;
    addr_ = addr;

    mask_  = 0;
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
    // loop_->append_event(this, events);
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

int32_t EventSwapList::append_event(EVIO *w, int32_t ev, int32_t &counter,
                                    int32_t &index)
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
