#include "ev_watcher.hpp"
#include "net/net_compat.hpp"

static_assert(offsetof(EVIO, ev_) / 64 != offsetof(EVIO, mask_) / 64,
    "atomic false sharing check");

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

}

EVIO::~EVIO()
{
    assert(0 == (mask_ & M_ALL_REF));

    if (io_) delete io_;
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

    return io_->do_init_accept(this);
}

int32_t EVIO::do_init_connect()
{
    // lua报错，在启动socket对象后无法正常设置io参数
    if (!io_) return EV_ERROR;

    return io_->do_init_connect(this);
}

bool EVIO::del_ref(int32_t b)
{
    int32_t v = mask_.fetch_and(~b);
    if (0 != (v & (~b) & M_ALL_REF)) return true;

    /**
     * 网络线程和逻辑线程共用一个watcher指针，通过mask_的ref来判断被哪些线程引用
     * 当最后一个线程解除引用时，关闭fd并销毁自己，没办法指定在逻辑线程销毁
     * 
     * 因为网络线程收到远端关闭时，如果先派发事件，逻辑线程处理事件时网络线程还未
     * 解除引用，无法销毁。如果网络线程先解除引用，则主线程也可能解除引用并销毁导
     * 致无法派发事件
     */
    if (fd_ != netcompat::INVALID) netcompat::close(fd_);
    delete this;

    return false;
}
