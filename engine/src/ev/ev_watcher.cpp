#include "ev_watcher.hpp"
#include "net/net_compat.hpp"

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

    ref_ = 0;

}

EVIO::~EVIO()
{

    assert(0 == ref_);

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

bool WatcherMgr::try_delete_watcher(EVIO *w)
{
    // 正常情况backend线程不会销毁watch
    // 但如果watcher所属于线程来不及处理，backend负责收尾
    if (w->del_ref(EVIO::REF_BACKEND))
    {
        PLOG("backend delete watcher, id = %d, fd = %d", w->id_, w->fd_);
        netcompat::close(w->fd_);
        delete w;
        return true;
    }
    return false;
}