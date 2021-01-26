#include <signal.h>
#include "thread.hpp"
#include "../system/static_global.hpp"

std::atomic<int32_t> Thread::_id_seed(0);

Thread::Thread(const char *name)
{
    _name   = name;
    _id     = ++_id_seed;
    _status = S_NONE;
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
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);

    pthread_sigmask(SIG_BLOCK, &mask, NULL);
}

/* 开始线程 */
bool Thread::start(int32_t us)
{
    mark(S_RUN);
    _thread = std::thread(&Thread::spawn, this, us);

    StaticGlobal::thread_mgr()->push(this);

    return true;
}

/* 终止线程 */
void Thread::stop()
{
    if (!active())
    {
        ERROR("thread::stop:thread not running");
        return;
    }

    unmark(S_RUN);
    _thread.join();

    StaticGlobal::thread_mgr()->pop(_id);
}

/* 线程入口函数 */
void Thread::spawn(int32_t us)
{
    signal_block(); /* 子线程不处理外部信号 */

    if (!initialize()) /* 初始化 */
    {
        ERROR("thread initialize fail");
        return;
    }

    std::chrono::microseconds timeout(us);
    while (_status & S_RUN)
    {
        // unique_lock在构建时会加锁，然后由wait_for解锁并进入等待
        // 当条件满足时，wait_for返回并加锁，最后unique_lock析构时解锁
        // TODO 把lock放for外面，这里手动lock会不会高效一些
        std::unique_lock<std::mutex> ul(_mutex);

        // 这里可能会出现spurious wakeup(例如收到一个信号)，但不需要额外处理
        // 目前所有的子线程唤醒多次都没有问题，以后有需求再改
        _cv.wait_for(ul, timeout);

        ul.unlock(); // 手动把锁释放掉，因为处理逻辑过程中需要根据粒度自已加锁解锁

        mark(S_BUSY);
        this->routine();
        unmark(S_BUSY);
    }

    if (!uninitialize()) /* 清理 */
    {
        ERROR("thread uninitialize fail");
        return;
    }
}
