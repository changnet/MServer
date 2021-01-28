#pragma once

/*
https://en.cppreference.com/w/cpp/thread/condition_variable

The condition_variable class is a synchronization primitive that can be used to
block a thread, or multiple threads at the same time, until another thread both
modifies a shared variable (the condition), and notifies the condition_variable

Even if the shared variable is atomic, it must be modified under the mutex in
order to correctly publish the modification to the waiting thread

http://www.cplusplus.com/reference/condition_variable/condition_variable/notify_one/
notify_one
If no threads are waiting, the function does nothing

```cpp
std::atomic<bool> dataReady{false};

void child(){
    while (true)
    {
        std::unique_lock<std::mutex> lck(mutex_);
        condVar.wait(lck, 1000ms);

        do_something();
    }
}

void main(){
    dataReady = true;
    std::cout << "Data prepared" << std::endl;
    condVar.notify_one();
}

int main(){
  std::thread t1(child);
  std::thread t2(main);
}
```
上面的代码是错的，因为子线程在执行`do_something`时，如果主线程发了notify_one，没有任何效果。
接着子线程进入wait，但永远不会收到notify。

上面的例子中，dataReady这个变量即文档中的`shared
variable`，atomic只能保证读写是安全的，但 和condition_variable配合使用的时候必须加锁(it
must be modified under the mutex)，否则就会出现上面那种子线程根本不会唤醒的情况。
```cpp
{
    std::lock_guard guard(m);
    dataReady = true;
}
```cpp
t2加锁后，由于t1在执行过程中一直持有锁，因此t2肯定必须等待t1重新进入wait后才能获得锁，才能
设置dataReady，可解决上面那种情况。

参考：https://www.nextptr.com/question/qa1456812373/move-stdunique_lock-to-transfer-lock-ownership

////////////////////////////////////////////////////////////////////////////////

```cpp
std::mutex m;
std::vector<job> jobs;

void do_something()
{
    do
    {
        m.lock();
        if (jobs.empty())
        {
            m.unlock();
            return;
        }
        auto job = jobs.front();
        jobs.pop_front();
        m.unlock();

        do_job(job); // 耗时很长，需要释放锁
    }
}

void child(){
    while (_run)
    {
        std::unique_lock<std::mutex> lck(m);
        condVar.wait(lck, 1000ms);

        lck.unlock(); // do_something耗时很长，因此释放锁，允许主线程写入数据
        do_something();
    }
}
```
上面的代码是错的，有两个问题
1. 在同一个线程中，使用unique_lock获得mutex的归属后，不允许再使用mutex直接加锁、解锁。
unique_lock在构造时，对mutex加锁，并设置owner标记为true，如果在标记为true，析构时执行
unlock。如果这时直接调用mutex.unlock，unique_lock中的owner标记就不正常。

2. t1在执行do_something时，不能解锁。do_something解锁后，t2线程可能会写入jobs队列。虽然
一般情况下，do_something中的循环会检测jobs队列是否为空。但临界情况下，t1确认jobs为空后，
解锁，然后退出do_something函数(执行一些析构之类)，这时刚好t2写入了数据并notify_one，但是
t1没有收到，执行完do_something后直接wait，无法处理队列中的数据。

**需要保证在检查完队列后，t1一直加锁直到进入wait**
```cpp
void do_something(std::unique_lock<std::mutex> &lck)
{
    do
    {
        // 保证检测完jobs为空后，依然持有锁进入wait状态
        if (jobs.empty())
        {
            return;
        }
        auto job = jobs.front();
        jobs.pop_front();

        lck.unlock();
        do_job(job); // 耗时很长，需要释放锁
        lck.lock();
    }
}
void child()
{
    std::unique_lock<std::mutex> lck(m);
    while (_run)
    {
        condVar.wait(lck, 1000ms);
        do_something(); // 保持加锁状态
    }
}
```
*/

#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>

#include "../ev/ev_watcher.hpp"
#include "../global/global.hpp"

/// 线程
class Thread
{
public:
    /// 线程的状态，按位取值
    enum Status
    {
        S_NONE  = 0,
        S_EXIT  = 1,   /// 子线程退出
        S_ERROR = 2,   /// 子线程发生错误
        S_RUN   = 4,   /// 子线程是否已启动
        S_JOIN  = 16,  /// 子线程是否已join
        S_BUSY  = 32,  /// 子线程是否繁忙
        S_WAIT  = 64,  /// 当关服的时候，是否需要等待这个线程
        S_MDATA = 128, /// 主线程有数据需要处理
        S_SDATA = 256, /// 子线程有数据需要处理
    };

public:
    virtual ~Thread();
    explicit Thread(const char *name);

    /// 注册信号处理
    static void signal(int32_t sig, int32_t action);
    /// 获取信号掩码并重置原有信号掩码
    static int32_t signal_mask_once()
    {
        int32_t sig_mask = _sig_mask;

        _sig_mask = 0;
        return sig_mask;
    }

    /// 停止线程
    void stop();
    /**
     * 启动线程，可设置多长时间超时一次
     * @param usec 微秒
     */
    bool start(int32_t us = 5000000);
    /**
     * 还在处理的数据
     * @param finish 是子线程已处理完，等待主线程处理的数量
     * @param unfinished 是等待子线程处理的数量
     * 返回总数
     */
    virtual size_t busy_job(size_t *finished   = nullptr,
                            size_t *unfinished = nullptr) = 0;

    /// 线程当前是否正在执行
    inline bool active() const { return _status & S_RUN; }
    /// 子线程是否正在处理数据，不在处理数据也不代表缓冲区没数据等待处理
    inline bool is_busy() const { return _status & S_BUSY; }
    /// 获取自定义线程id
    inline int32_t get_id() const { return _id; }
    /// 获取线程名字
    inline const char *get_name() const { return _name; }

    inline bool is_wait_busy() const { return _status & S_WAIT; }
    inline void set_wait_busy(bool busy)
    {
        _status = busy ? _status | S_WAIT : _status & (~S_WAIT);
    }

    static void signal_block();

protected:
    /// 标记状态
    void mark(int32_t status) { _status |= status; }
    /// 取消状态
    void unmark(int32_t status) { _status &= ~status; }
    /// 唤醒子线程
    void wakeup()
    {
        // https://en.cppreference.com/w/cpp/thread/condition_variable/notify_one
        // The notifying thread does not need to hold the lock on the same mutex
        // as the one held by the waiting thread(s); in fact doing so is a
        // pessimization
        // std::lock_guard<std::mutex> guard(_mutex);
        _cv.notify_one();
    }

    virtual bool initialize() { return true; }   /* 子线程初始化 */
    virtual bool uninitialize() { return true; } /* 子线程清理 */

    /// 加锁，只能在主线程调用
    inline void lock()
    {
        assert(std::this_thread::get_id() != _thread.get_id());
        _mutex.lock();
    }
    /// 解锁，只能在主线程调用
    inline void unlock()
    {
        assert(std::this_thread::get_id() != _thread.get_id());
        _mutex.unlock();
    }

    // 子线程逻辑(注意执行该函数已持有锁，如果执行耗时操作需要解锁)
    virtual void routine(std::unique_lock<std::mutex> &ul) = 0;
    // 主线程逻辑
    virtual void main_routine() {}

private:
    void spawn(int32_t us);
    static void sig_handler(int32_t signum);

private:
    int32_t _id;                  /// 线程自定义id
    const char *_name;            /// 线程名字，日志用而已
    std::atomic<int32_t> _status; /// 线程状态，参考 Status 枚举

    std::mutex _mutex;
    std::thread _thread;
    std::condition_variable _cv;

    /// 各线程收到的信号统一存这里，由主线程处理
    /// 一般的做法是子线程屏蔽信号，由主线程接收，但c++标准里不提供屏蔽的接口
    static std::atomic<uint32_t> _sig_mask;

    // 用于产生自定义线程id
    // std::thread::id不能保证为数字(linux下一般为pthread_t)，不方便传参
    static std::atomic<int32_t> _id_seed;
};
