#include <csignal>
#include "thread.hpp"
#include "../system/static_global.hpp"

std::atomic<int32_t> Thread::_sig_mask(0);
std::atomic<int32_t> Thread::_id_seed(0);

Thread::Thread(const std::string &name)
{
    _name   = name;
    _id     = ++_id_seed;
    _status = S_NONE;

    _ev      = 0;
    _main_ev = 0;

    set_wait_busy(true); // 默认关服时都是需要待所有任务处理才能结束
}

Thread::~Thread()
{
    assert(!active());
    assert(!_thread.joinable());
}

void Thread::signal_block()
{
    /* 信号阻塞
     * 多线程中需要处理三种信号：
     * 1)pthread_kill向指定线程发送的信号，只有指定的线程收到并处理
     * 2)SIGSEGV之类由本线程异常的信号，只有本线程能收到。但通常是终止整个进程。
     * 3)SIGINT等通过外部传入的信号(如kill指令)，查找一个不阻塞该信号的线程。如果有多个，
     *   则选择第一个。
     * 因此，对于1，当前框架并未使用。对于2，默认abort并coredump。对于3，我们希望主线程
     * 收到而不希望子线程收到，故在这里要block掉。
     */

    /* TODO C++标准在csignal中提供了部分信号处理的接口，但没提供屏蔽信号的
     * linux下，使用pthread的接口应该是兼容的，但无法保证所有std::thread的实现都使用
     * pthread。因此不屏蔽子线程信号了，直接存起来，由主线程定时处理
     */

    /*
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);

    pthread_sigmask(SIG_BLOCK, &mask, NULL);*/
}

void Thread::sig_handler(int32_t signum)
{
    // 把有线程收到的信号存这里，由主线程处理

    // 这个函数必须是 async-signal-safe function
    // https://man7.org/linux/man-pages/man7/signal-safety.7.html

    // 这意味这这函数里不能使用mutex之类的锁，不然第一个信号加锁后，第二个信号把
    // 第一个信号挂起加锁，就会形成死锁

    // std::atomic在lock_free的情况下是async-signal-safe，x86架构一般都是lock_free

    // signal.h #define _NSIG            64
    // 但一般只处理前31个
    if (signum > 31) return;
   
   int32_t old = _sig_mask;
    _sig_mask |= (1 << signum);

    // C++的condition_variable不是async-signal-safe的，因此这里可能有点问题
    // 方案1是不使用signal_handler，而是所有线程都屏蔽信号
    //      使用一个独立的线程调用sigwait，这样该线程就是一个普通的线程，可以
    //      安全使用condition_variable唤醒主线程，但这个比较复杂且win下不好实现
    //      参考：https://thomastrapp.com/blog/signal-handler-for-multithreaded-c++/
    // 
    // 这里暂时用_sig_mask判断一下，如果不为0说明已经唤醒过了，不需要再次唤醒
    // 当然这个不是很准，可能设置完_sig_mask的值另一线程重新进入睡眠了
    // 由于这里信号使用很少，未生效可以多次发
    if (old) return;

    StaticGlobal::ev()->set_job(true);
    StaticGlobal::ev()->wake_safe();
}

void Thread::signal(int32_t sig, int32_t action)
{
    /**
     * https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/signal?view=msvc-160
     * win下支持signal，只是支持不是很好
     * https://docs.microsoft.com/en-us/windows/console/handlerroutine
     * SetConsoleCtrlHandler这一套，才是win下原生的事件，测试后发现ctrl c、ctrl
     * break这两个在signal和SetConsoleCtrlHandler 中都能触发，可以用
     * https://docs.microsoft.com/en-us/windows/console/registering-a-control-handler-function
     * 来测试
     *
     * taskkill会在SetConsoleCtrlHandler中产生一个CTRL_CLOSE_EVENT事件，但这个事件如果返回false，则操作系统查找下一个处理
     * 信号的函数并调用，直到返回true，没找到默认就会终止进程。如果返回true，则操作系统会直接终止进程。无论如何，这个事件无法
     * 屏蔽并且最终都会终止进程，如果在进程里sleep，5秒后操作系统也会直接终止进程。
     *
     * 使用powershell的stop-process来终止程序，则不会收到任何事件、信号，进程直接就退出了
     *
     * 除了在控制台按ctrl c，目前win下没找到任何办法向一个进程发送ctrl
     * c信号。一些第三方的工具(windows-kill、SendSignal等，
     * 都是attach到进程的console，通过GenerateConsoleCtrlEvent来产生依赖)，但start进程的方式不一样(例如
     * /b 参数)，这些工具 经常不适用
     *
     * 因此，目前win下没办法像linux下那样通过脚本来安全kill掉进程，taskkill那5秒的时间一般也不足以关掉一个游戏服务器
     * 所以CTRL_CLOSE_EVENT这个事件这里也不处理了
     *
     * node.js(libuv)的处理方式：https://github.com/libuv/libuv/blob/master/src/win/signal.c
     */
    /* check /usr/include/bits/signum.h for more */
    if (0 == action)
    {
        ::signal(sig, SIG_DFL);
    }
    else if (1 == action)
    {
        ::signal(sig, SIG_IGN);
    }
    else
    {
        ::signal(sig, sig_handler);
    }
}

/* 开始线程 */
bool Thread::start(int32_t us)
{
    PLOG("%s thread start", _name.c_str());

    // 只能在主线程调用，threadmgr没加锁
    assert(std::this_thread::get_id() != _thread.get_id());

    mark(S_RUN);
    _thread = std::thread(&Thread::spawn, this, us);

    StaticGlobal::thread_mgr()->push(this);

    return true;
}

/* 终止线程 */
void Thread::stop()
{
    PLOG("%s thread stop", _name.c_str());

    // 只能在主线程调用，thread_mgr没加锁
    assert(std::this_thread::get_id() != _thread.get_id());
    if (!active())
    {
        ELOG("thread::stop:thread not running");
        return;
    }

    unmark(S_RUN);
    wakeup(S_RUN);
    _thread.join();

    StaticGlobal::thread_mgr()->pop(_id);
}

/* 线程入口函数 */
void Thread::spawn(int32_t us)
{
    signal_block(); /* 子线程不处理外部信号 */

    if (!initialize()) /* 初始化 */
    {
        ELOG("%s thread initialize fail", _name.c_str());
        return;
    }

    std::chrono::microseconds timeout(us);
    {
        // unique_lock在构建时会加锁，然后由wait_for解锁并进入等待
        // 当条件满足时，wait_for返回并加锁，最后unique_lock析构时解锁
        std::unique_lock<std::mutex> ul(_mutex);
        while (_status & S_RUN)
        {
            int32_t ev = _ev.exchange(0);
            if (!ev)
            {
                // 这里可能会出现spurious wakeup(例如收到一个信号)，但不需要额外处理
                // 目前所有的子线程唤醒多次都没有问题，以后有需求再改
                _cv.wait_for(ul, timeout);
            }

            ul.unlock();
            mark(S_BUSY);
            this->routine(ev);
            unmark(S_BUSY);
            ul.lock();
        }
    }

    if (!uninitialize()) /* 清理 */
    {
        ELOG("%s thread uninitialize fail", _name.c_str());
        return;
    }
}
