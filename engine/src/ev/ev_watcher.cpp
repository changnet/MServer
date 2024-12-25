#include "ev_watcher.hpp"

EVIO::EVIO(int32_t id, int32_t addr, int32_t fd)
{
    fd_   = fd;
    id_   = id;
    addr_ = addr;

    mask_  = 0;
    errno_ = 0;

    ev_ = 0;

    b_kevents_ = 0;
    b_pevents_ = 0;

    b_ev_ = 0;

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

