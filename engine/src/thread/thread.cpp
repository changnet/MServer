#include <csignal>
#include "thread.hpp"
#include "system/static_global.hpp"
#ifdef __linux__
#include <sys/prctl.h> // for prctl
#endif

Thread::Thread(const std::string &name)
{
    name_   = name;
    stop_   = true;
}

Thread::~Thread()
{
    assert(stop_);
    assert(!thread_.joinable());
}

void Thread::apply_thread_name(const char *name)
{
    // 设置线程名字，只用用于调试。VS在中断时，可以方便地根据线程名知道属于哪个线程。
    // 在linux下，top -H -p xxxx（不要加-c参数）可以查看各个线程占用的cpu
    // 测试中发现，假如线程A的名字为foo，线程A再去启动其他线程时
#ifdef __windows__
    // https://learn.microsoft.com/en-us/visualstudio/debugger/tips-for-debugging-threads?view=vs-2022&tabs=csharp
    // There are two ways to set a thread name. The first is via the SetThreadDescription function. 
    // The second is by throwing a particular exception while the Visual Studio debugger is attached 
    // to the process. Each approach has benefits and caveats. The use of SetThreadDescription is 
    // supported starting in Windows 10 version 1607 or Windows Server 2016.
    #if !(NTDDI_WIN10_VB && NTDDI_VERSION >= NTDDI_WIN10_VB)
    const DWORD MS_VC_EXCEPTION = 0x406D1388;
    #pragma pack(push, 8)
    typedef struct tagTHREADNAME_INFO
    {
        DWORD dwType;     // Must be 0x1000.
        LPCSTR szName;    // Pointer to name (in user addr space).
        DWORD dwThreadID; // Thread ID (-1=caller thread).
        DWORD dwFlags;    // Reserved for future use, must be zero.
    } THREADNAME_INFO;
    #pragma pack(pop)

    THREADNAME_INFO info;
    info.dwType     = 0x1000;
    info.szName     = threadName;
    info.dwThreadID = GetCurrentThreadId();
    info.dwFlags    = 0;
    #pragma warning(push)
    #pragma warning(disable : 6320 6322)
    __try
    {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR),
                       (ULONG_PTR *)&info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    #pragma warning(pop)
    #else
    const static int32_t SIZE = 256;
    WCHAR wname[SIZE];
    int32_t e = MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, SIZE);
    assert(0 != e);
    HRESULT hr = SetThreadDescription(GetCurrentThread(), wname);
    if (FAILED(hr))
    {
        ELOG("SetThreadDescription fail:%s", name); // Call failed.
    }
    #endif
#else
    // linux下,prctl应该是直接调用pthread_setname_np
    prctl(PR_SET_NAME, name, 0, 0, 0);
#endif
}

bool Thread::start(int32_t ms)
{
    // 刚启动时，会启动一些底层线程，这时候日志设置还没好（日志路径未设置好）
    // PLOG("%s thread start", name_.c_str());

    // 只能在主线程调用
    assert(std::this_thread::get_id() != thread_.get_id());

    // 这个线程已经在运行中
    if (thread_.joinable())
    {
        ELOG("thread %s already active", name_.c_str());
        return false;
    }
    thread_ = std::thread(&Thread::spawn, this, ms);

    return true;
}

void Thread::stop(bool join)
{
    stop_ = true;

    if (join && thread_.joinable())
    {
        // 只能在主线程调用
        assert(std::this_thread::get_id() != thread_.get_id());
        cv_.notify_one(1);

        thread_.join();
    }
}

void Thread::routine(int32_t ms)
{
    while (likely(!stop_))
    {
        int32_t ev = cv_.wait_for(ms);
        routine_once(ev);
    }
}

void Thread::spawn(int32_t ms)
{
    // signal_block(); /* 子线程不处理外部信号 */

    apply_thread_name(name_.c_str());

    if (!initialize()) /* 初始化 */
    {
        ELOG("%s thread initialize fail", name_.c_str());
        return;
    }

    stop_ = false;
    routine(ms);
    stop_ = true;

    if (!uninitialize()) /* 清理 */
    {
        ELOG("%s thread uninitialize fail", name_.c_str());
        return;
    }
}
