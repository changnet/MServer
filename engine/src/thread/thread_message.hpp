#pragma once

#include <deque>
#include <mutex>
#include "global/global.hpp"

// 线程数据交互结构
struct ThreadMessage
{
    int32_t addr_; //  worker地址
    void *udata_; // 自定义数据指针，可能是buff也可能是其他指针
};

// 用于数据交换的列表
class ThreadMessageQueue
{
public:
    ThreadMessage* pop()
    {
        std::lock_guard<std::mutex> lg(mutex_);

        ThreadMessage *message = queue_.front();
        queue_.pop_front();

        return message;
    }
    void push(ThreadMessage* message)
    {
        std::lock_guard<std::mutex> lg(mutex_);
        queue_.emplace_back(message);
    }

private:
    std::mutex mutex_;
    std::deque<ThreadMessage *> queue_;
};
