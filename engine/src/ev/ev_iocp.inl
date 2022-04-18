const char *__BACKEND__ = "IOCP";

#include <thread>

/// backend using iocp implement
class FinalBackend final : public EVBackend
{
public:
    FinalBackend();
    ~FinalBackend();

    bool stop();
    bool start(class EV *ev);
    void wake();
    void backend();
    void modify(int32_t fd, EVIO *w);

private:
    /**
     * @brief iocp子线程主函数
    */
    void iocp_routine();

private:
    HANDLE _h_iocp; // global iocp handle
    std::vector<std::thread> _threads;
};

 FinalBackend::FinalBackend()
{
}

FinalBackend::~FinalBackend()
{
}

bool FinalBackend::stop()
{
    return true;
}

bool FinalBackend::start(class EV *ev)
{
    // 游戏服务器io压力不大，不需要使用太多的线程
    // If this parameter is zero, the system allows as many concurrently running threads as there are processors in the system
    /*
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    const DWORD thread_num = sys_info.dwNumberOfProcessors * 2;
    */
    const DWORD thread_num = 2;
    _h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, thread_num);
    if (!_h_iocp)
    {
        ELOG("iocp start fail: " FMT64u, GetLastError());
        return false;
    }

    for (DWORD i = 0; i < thread_num; ++i)
    {
        _threads.emplace_back(&FinalBackend::iocp_routine, this);
    }

    return true;
}

void FinalBackend::iocp_routine()
{
}

void FinalBackend::wake()
{
}
void FinalBackend::backend()
{
}

void FinalBackend::modify(int32_t fd, EVIO *w)
{
}