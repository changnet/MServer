#pragma once

#include <deque>
#include <mutex>
#include "global/global.hpp"

// 线程数据交互结构
struct ThreadMessage
{
    enum
    {
        TIMER  = 1, // 定时器
        SIGNAL = 2, // 信号
    };

    // 构造函数
    ThreadMessage(int32_t src, int32_t dst, int32_t type, void *udata,
                  int32_t usize)
    {
        src_ = src;
        dst_ = dst;
        type_ = type;
        usize_ = usize;

        if (udata)
        {
            udata_ = new char[usize];
            memcpy(udata_, udata, usize);
        }
        else
        {
            udata_ = nullptr;
        }
    }
    ~ThreadMessage()
    {
        // TODO 这个udata它可能来源于某个内存池，有可能来源于new分配
        // 所以不要在析构里处理它，需要手动调用dispose
        // 尤其是消息队列使用对象时，pop一个对象给Lua就会调用析构函数，但实际
        // 该数据还不需要释放
        // 或者：根据type_类型或者做一个标识来处理
    }
    void dispose()
    {
        if (udata_) delete[] udata_;
    }

    int32_t src_; // 来源地址
    int32_t dst_;  // 目标地址
    int32_t type_; // 消息类型
    int32_t usize_; // 自定义数据长度
    void *udata_; // 自定义数据，可能是buff也可能是其他指针

    static ThreadMessage Null;
};

// 用于数据交换的队列
class ThreadMessageQueue
{
public:
    // 从消息队列中弹出第一个消息。如果队列为空，则抛出一个out_of_range
    ThreadMessage pop()
    {
        std::lock_guard<std::mutex> lg(mutex_);

        if (queue_.empty())
        {
            // too slow
            // throw std::out_of_range("empty queue");
            return ThreadMessage::Null;
        }

        // ThreadMessage只有基础类型，std::mvoe现在没啥意义
        ThreadMessage message = std::move(queue_.front());
        queue_.pop_front();

        return message; // copy elision
    }

    template <typename... Args> void emplace(Args &&...args)
    {
        std::lock_guard<std::mutex> lg(mutex_);
        queue_.emplace_back(std::forward<Args>(args)...);
    }
    void push(ThreadMessage& message)
    {
        std::lock_guard<std::mutex> lg(mutex_);
        queue_.emplace_back(message);
    }

private:
    std::mutex mutex_;

    /**
     * @brief 当前线程的消息队列
     * deque内部采用多个大块（类似vector）内存连接的实现，保证不会出现太多内存碎片
     * 另一种方式是做一个全局的message_pool，不过需要加锁管理
     */
    std::deque<ThreadMessage> queue_;
};
