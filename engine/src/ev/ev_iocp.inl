/**
 * @brief 尝试过用IOCP实现，但难度比较大
 * 这框架原本是按Linux的api来实现的，用的connect、accept、send、recv等函数
 * 
 * 但如果用IOCP的话，就要用WSASend、WSARecv、acceptEx等函数，要做兼容的话修改太
 * 大
 * 
 * 另外，OpenSSL那边现在也是根据fd来操作的，用IOCP的话，需要重新用BIO的方式来
 * 实现，把fd读写和缓冲区操作分开
 * 
*/

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
    _h_iocp = 0;
}

FinalBackend::~FinalBackend()
{
}

bool FinalBackend::stop()
{
    if (!_h_iocp) return true;

    // 通知所有iocp子线程退出
    for (size_t i = 0; i < _threads.size();i ++)
    {
        if (0 ==PostQueuedCompletionStatus(_h_iocp, 0, 0, nullptr))
        {
            ELOG("stop iocp thread fail: " FMT64u, GetLastError());
        }
    }

    // 等待子线程退出
    for (auto &thd : _threads)
    {
        thd.join();
    }

    // 关闭iocp句柄
    CloseHandle(_h_iocp);

    return true;
}

bool FinalBackend::start(class EV *ev)
{
    /**
     * 游戏服务器io压力不大，不需要使用太多的线程
     * 
     * https://stackoverflow.com/questions/38133870/how-the-parameter-numberofconcurrentthreads-is-used-in-createiocompletionport
     * https://msdn.microsoft.com/en-us/library/windows/desktop/aa365198.aspx
     * 
     * @param thread_num
     * If this parameter is zero, the system allows as many concurrently running threads as there are processors in the system
     * 这个数量指当有数据需要处理时，iocp分配的线程数，而不是创建的线程数
     * 假如这个数值为1，后面创建了2条线程调用GetQueuedCompletionStatus，也只会
     * 分配1线程工作
     */

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
    const DWORD ms = 2000; // ms = INFINITE;

    while (true)
    {
        OVERLAPPED *overlapped = nullptr;
        ULONG_PTR key          = 0;
        DWORD bytes            = 0;
        int ok = GetQueuedCompletionStatus(_h_iocp, &bytes, &key, &overlapped, ms);

        /**
         * https://docs.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatus
         * 
         * 1. If a call to GetQueuedCompletionStatus fails because the completion port handle associated with it is closed while the call is outstanding, the function returns FALSE, *lpOverlapped will be NULL, and GetLastError will return ERROR_ABANDONED_WAIT_0
         */

        // 1. _h_iocp关闭
        // 2. 读写事件
        // 添加、移除写事件
        // socket关闭或者出错

        // 关闭时，stop函数里PostQueuedCompletionStatus传key值0
        // 或者_h_iocp强制关闭：If a call to GetQueuedCompletionStatus fails because the completion port handle associated with it is closed while the call is outstanding, the function returns FALSE, *lpOverlapped will be NULL, and GetLastError will return ERROR_ABANDONED_WAIT_0
        if (0 == key || !overlapped) break;


    }
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