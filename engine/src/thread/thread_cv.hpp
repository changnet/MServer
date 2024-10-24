#pragma once

/*
传统的socketpair、pipe通信，无论什么时候往里写入数据，另一个线程在任何时候去读，
必定能把数据读出来。

但condition_variable不一样，它只是单纯的wait，不具备把数据缓存起来，等下次处理的
功能。即当前它不处于wait状态时，另一个线程的notify是直接丢掉的。那稍后再进入wait
状态是不会再收到已丢掉的notify。

```cpp
void child(){

        std::unique_lock<std::mutex> lck(mutex_);
        cv.wait(lck, 1000ms);
}

void main(){
    cv.notify_one();
}

int main(){
  std::thread t1(child);
  std::thread t2(main);
  t1.join();
  t2.join();
}
```
上面的代码中，可能会出现t2线程执行notify_one时，t1线程尚未运行到wait状态，所以
这个notify_one是无任何效果。当t1线程最终运行到wait状态时，却再也不会收到notify，
整个程序就只能永远等待下去

为了解决这个问题，通常会使用一个额外的变量来辅助是否需要wait

```cpp
bool ready = false;
std::mutex mutex_;
void child(){
        {
            std::unique_lock<std::mutex> lck(mutex_);
            if (!ready) cv.wait(lck, 1000ms);
         }
        // do something
}
void main(){
    {
        std::unique_lock<std::mutex> lck(mutex_);
        ready = true;
     }
    cv.notify_one();
}
int main(){
  std::thread t1(child);
  std::thread t2(main);
  t1.join();
  t2.join();
}
```
上面的用法有几个要点
1. ready的设置和读取必须要加锁，即使ready本身为atomic也需要加锁。这是因为要保证
ready和wait这个操作为线程安全的。如果不加锁，rady和wait会被分为两部分
```cpp
if (!ready) // 假如ready是atomic，这里读取没问题
{
    // 但是在进入wait之前，不加锁其他线程可能刚好设置了ready，并且发了notify
    std::unique_lock<std::mutex> lck(mutex_);
    cv.wait(lck, 1000ms);
}
```
2.上面已经说了，wait之前必须要加锁。但进入wait之后，会内部解锁，退出wait时，又会
内部加锁，所以后续逻辑需要在解锁执行的话，千万不要和lock放在同一生命周期。
```cpp
{
   std::unique_lock<std::mutex> lck(mutex_);
   if (!ready) cv.wait(lck, 1000ms);
   // wait放一个独立的block，析构时会解锁，后续逻辑以不加锁状态执行
}
// do something
```

3. 调用notify_one等notify函数，不要加锁。修改ready时加锁，就能保证另一端的线程
ready和wait是线程安全的了。notify_one会唤醒另一个线程，如果这时加锁的话，被唤醒
的线程退出wait时，没法获取锁，反而会进入竞争状态直到notify线程把锁释放，性能反而
下降
https://en.cppreference.com/w/cpp/thread/condition_variable/notify_one
The notifying thread does not need to hold the lock on the same mutex
as the one held by the waiting thread(s); in fact doing so is a pessimization
https://stackoverflow.com/questions/17101922/do-i-have-to-acquire-lock-before-calling-condition-variable-notify-one

////////////////////////////////////////////////////////////////////////////////
!!!!!!!! 关于线程唤醒的多种方式
子线程是单任务的（写文件、操作数据库等），因此可以用阻塞的方式等待数据，采用socket、
socketpair、condition_variable问题都不大。但当子线程返回数据给主线程时，主线程是多任务的，
可能处理于epoll_wait，无法用condition_variable来唤醒。

旧版本(pthread版本)使用socketpair来唤醒主线程。现在使用std::thread后，由于socketpair不是
C++标准的内容，改用busy wait的方式，即主线程每5ms查询一次子线程的数据。

改用busy wait后，空载CPU会上涨1%左右
*/

#include <mutex>
#include <condition_variable>
#include "global/global.hpp"

/// 重新封装std::condition_variable，实现线程的唤醒功能
class ThreadCv
{
public:
    ThreadCv()
    {
        _data = 0;
    }
    ~ThreadCv(){}

    /**
     * @brief 唤醒等待的一条线程
     * @param data 自定义数据，不为0即可。多次唤醒的数据会按位叠加
     */
    void notify_one(int32_t data)
    {
        {
            std::unique_lock<std::mutex> ul(_mutex);
            if (_data)
            {
                _data |= data;
                return;
            }
            _data |= data;
        }
        _cv.notify_one();
    }
    /**
     * @brief 唤醒所有等待的线程
     * @param data 自定义数据，不为0即可。多次唤醒的数据会按位叠加
     */
    void notify_all(int32_t data)
    {
        {
            std::unique_lock<std::mutex> ul(_mutex);
            _data |= data;
        }
        _cv.notify_all();
    }
    /**
     * @brief 等待N毫秒
     * @param ms 等待的毫秒数
     * @return 收到的自定义数据。0表示等待超时
     */
    int32_t wait_for(int64_t ms)
    {
        std::unique_lock<std::mutex> ul(_mutex);
        if (!_data) _cv.wait_for(ul, std::chrono::milliseconds(ms));

        int32_t data = _data;
        _data        = 0;

        // std::cv_status::no_timeout 表示不了这么数据，所以才返回int32_t表示
        return data;
    }

private:
    // 所属线程需要处理的数据
    int32_t _data;
    std::mutex _mutex;
    // TODO C＋＋20可以wait一个atomic变量，到时优化一下
    std::condition_variable _cv;
};
