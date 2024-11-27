#pragma once

#include <atomic>

/**
 * https://www.cnblogs.com/coding-my-life/p/15779082.html
 * C++ std中暂时没有spinlock，需要自己实现
 * pthread_spinlock_t性能更高一些，但并不通用
 * 
 * 但实际上，spinlock并没比std::mutex好多少（不包是自己实现的spinlock还是pthread_spinlock）
 * 尤其是cpu非常紧张的时候，spinlock的效率更差
 * 
 * https://stackoverflow.com/questions/61526837/why-is-there-no-std-equivalent-to-pthread-spinlock-t-like-there-is-for-pthread
 * Spinlocks are often considered a wrong tool in user-space because there is no
 * way to disable thread preemption while the spinlock is held (unlike in kernel).
 * So that a thread can acquire a spinlock and then get preempted, causing all 
 * other threads trying to acquire the spinlock to spin unnecessarily (and if 
 * those threads are of higher priority that may cause a deadlock (threads waiting
 * for I/O may get a priority boost on wake up)). This reasoning also applies to
 * all lockless data structures, unless the data structure is truly wait-free 
 * (there aren't many practically useful ones, apart from boost::spsc_queue)
 */

/// 用std::atomic_flag实现的spin lock
class SpinLock final
{
public:
    SpinLock()
    {
        _flag.clear();

        // C++20 This macro is no longer needed and deprecated,
        // since default constructor of std::atomic_flag initializes it to clear
        // state. _flag = ATOMIC_FLAG_INIT;
    }
    ~SpinLock()                 = default;
    SpinLock(const SpinLock &)  = delete;
    SpinLock(const SpinLock &&) = delete;

    void lock() noexcept
    {
        // https://en.cppreference.com/w/cpp/atomic/atomic_flag_test_and_set
        // Example A spinlock mutex can be implemented in userspace using an atomic_flag

        //test_and_set返回的是上一次的状态。如果返回true，则表示被其他线程占用
        while (_flag.test_and_set(std::memory_order_acquire))
            ;
    }

    bool try_lock() noexcept
    {
        return !_flag.test_and_set(std::memory_order_acquire);
    }

    void unlock() noexcept
    {
        _flag.clear(std::memory_order_release);
    }

private:
    std::atomic_flag _flag;
};
