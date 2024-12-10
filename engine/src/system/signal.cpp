#pragma once

#include "signal.hpp"
#include "static_global.hpp"
#include "global/platform.hpp"

/// 各线程收到的信号统一存这里，由主线程处理
static std::atomic<int32_t> sig_mask(0);

int32_t signal_mask_once()
{
    return sig_mask.exchange(0);
}

static void sig_handler(int32_t signum)
{
    // 把有线程收到的信号存这里，由主线程处理

    // 这个函数必须是 async-signal-safe function
    // https://man7.org/linux/man-pages/man7/signal-safety.7.html

    // 这意味这这函数里不能使用mutex之类的锁，不然第一个信号加锁后，第二个信号把
    // 第一个信号挂起加锁，就会形成死锁

    // std::atomic<int>在lock_free的情况下是async-signal-safe，x86架构一般都是lock_free
    // std::sig_atomic_t是能保证信号安全，但它不是线程安全


    // C++的condition_variable以及mutex都不是async-signal-safe的，因此这里调用加锁的函数唤醒另一个线程是可能会导致死锁的
    // https://www.man7.org/linux/man-pages/man7/signal-safety.7.html
    // 
    // 一个解决方案是线程使用pipe、socket、eventfd等async-signal-safe的机制来阻塞，这样wirte一个数据去唤醒线程是安全的。但这个不好跨平台
    // 
    // 另一个解决方案所有线程都屏蔽信号，然后创建一个独立的线程调用sigwait，这样该线程就是一个普通的线程，
    // 可以安全使用condition_variable唤醒主线程，但这个比较复杂且win下不好实现
    //      参考：https://thomastrapp.com/blog/signal-handler-for-multithreaded-c++/
    //
    // 但这些方案都太复杂
    // 
    // 用sig_mask判断一下，如果不为0说明已经唤醒过了，不需要再次唤醒，就不需要加锁，避免死锁
    // 当然这个不是很准，可能设置完sig_mask的值另一线程刚好处理完旧信号重新进入睡眠了，导致信号丢失
    // 考虑到信号是一个使用频率很低的功能，并且基本不影响业务逻辑，这里只要不死锁即可

    // signal.h #define _NSIG            64 但一般只处理前31个
    if (signum > 31) return;
    if (sig_mask.fetch_or(1 << signum)) return;

    StaticGlobal::M->push_message(ThreadMessage::SIGNAL, 0, 0, 0);
}

void signal_mask(int32_t sig, int32_t mask)
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
    if (0 == mask)
    {
        ::signal(sig, SIG_DFL);
    }
    else if (1 == mask)
    {
        ::signal(sig, SIG_IGN);
    }
    else
    {
        ::signal(sig, sig_handler);
    }
}

#ifdef __windows__
BOOL WINAPI win_console_handler(DWORD event)
{
    // https://learn.microsoft.com/zh-cn/windows/console/setconsolectrlhandler
    // https://learn.microsoft.com/zh-cn/windows/console/handlerroutine
    // event 的值为CTRL_BREAK_EVENT CTRL_CLOSE_EVENT等，但这里没必要区分
    sig_handler(SIGTERM);

    return true;
}
#endif


void mask_comm_signal()
{
    /* 信号阻塞
     * 多线程中需要处理三种信号：
     * 1)pthread_kill向指定线程发送的信号，只有指定的线程收到并处理
     * 2)SIGSEGV之类由本线程异常的信号，只有本线程能收到。但通常是终止整个进程。
     * 3)SIGINT等通过外部传入的信号(如kill指令)，查找一个不阻塞该信号的线程。如果有多个，
     *   则选择第一个。
     * 对于1，当前框架并未使用
     * 对于2，默认abort并coredump。
     * 对于3，我们希望主线程收到而不希望子线程收到，因此一般子线程要block掉这些信号
     */

    /* TODO C++标准在csignal中提供了部分信号处理的接口，但没提供屏蔽信号的
     * linux下，使用pthread_sigmask(SIG_BLOCK, &mask, NULL);应该是兼容的，但windows下的std::thread不行
     * 
     * 因此干脆不屏蔽了，把信号统一收集起来，等主线程处理
     */

    // 一些常用自定义的信号，等脚本处理
#ifdef __linux__
    // windows没有这些信号
    ::signal(SIGHUG, sig_handler);
    ::signal(SIGUSR1, sig_handler);
    ::signal(SIGUSR2, sig_handler);
#endif
#ifdef __windows__
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)win_console_handler, TRUE);
#endif
    ::signal(SIGINT, sig_handler);
    ::signal(SIGTERM, sig_handler);
}
