#pragma once

#include "global/platform.hpp"
#if defined(__poll__)

#include "ev_backend.hpp"

#include "net/net_compat.hpp"
#ifdef __windows__
    #include <SdkDdkVer.h>
#else
    #include <poll.h>
#endif

/// backend using poll implement
class PollBackend final : public EVBackend
{
public:
    PollBackend();
    ~PollBackend();

    void after_stop() override;
    bool before_start() override;
    void wake() override;

private:
    int32_t wait(int32_t timeout) override;
    void do_wait_event(int32_t ev_count) override;
    int32_t modify_fd(int32_t fd, int32_t op, int32_t new_ev) override;
    int32_t del_fd_index(int32_t fd);
    int32_t get_fd_index(int32_t fd);
    int32_t add_fd_index(int32_t fd);
    void update_fd_index(int32_t fd, int32_t index);

private:
    static const int32_t HUGE_FD = 10240;
    int32_t wake_fd_[2]; /// 用于唤醒子线程的fd
    std::vector<int32_t> fd_index_;
    std::unordered_map<int32_t, int32_t> fd_index_huge_;

#ifdef __windows__
    std::vector<WSAPOLLFD> poll_fd_;
#else
    std::vector<struct pollfd> poll_fd_;
#endif
};

#endif
