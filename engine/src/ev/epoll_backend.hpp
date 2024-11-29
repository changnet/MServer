#pragma once

#include "global/platform.hpp"
#if defined(__epoll__)

#include "net/net_compat.hpp"
#include "ev_backend.hpp"

#include <sys/epoll.h>

/// backend using epoll implement
class EpollBackend final : public EVBackend
{
public:
    EpollBackend();
    ~EpollBackend();

    void after_stop() override;
    bool before_start() override;
    void wake() override;

private:
    int32_t wait(int32_t timeout) override;
    void do_wait_event(int32_t ev_count) override;
    int32_t modify_fd(int32_t fd, int32_t op, int32_t new_ev) override;

private:
    int32_t ep_fd_;   /// epoll句柄
    int32_t wake_fd_; /// 用于唤醒子线程的fd

    /// epoll max events one poll
    /* https://man7.org/linux/man-pages/man2/epoll_wait.2.html
     * If more than maxevents file descriptors are ready when
     * epoll_wait() is called, then successive epoll_wait() calls will
     * round robin through the set of ready file descriptors.  This
     * behavior helps avoid starvation scenarios
     */
    static const int32_t EPOLL_MAXEV = 8192;

    epoll_event ep_ev_[EPOLL_MAXEV];
};

#endif
