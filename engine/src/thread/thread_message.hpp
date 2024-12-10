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

    union UserData
    {
        struct
        {
            int32_t u1_;
            int32_t u2_;
        };
        struct
        {
            void *ptr_;
        };
    };

    // 构造函数
    ThreadMessage(int32_t addr, int32_t type, int32_t u1, int32_t u2)
        : addr_(addr), type_(type), ud_{{u1, u2}}
    {
    }

    ThreadMessage(int32_t addr, int32_t type, void *ptr)
        : addr_(addr), type_(type)
    {
        ud_.ptr_ = ptr;
    }

    int32_t addr_; //  worker地址
    int32_t type_; // 消息类型
    UserData ud_; // 自定义数据，可能是buff也可能是其他指针

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
