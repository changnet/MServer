#pragma once

#include "thread.hpp"

/// 线程管理
class ThreadMgr
{
public:
    ThreadMgr();
    ~ThreadMgr();

    void pop(int32_t thd_id);
    void push(class Thread *thd);

    /// 停止所有线程
    void stop();

    /// 主线程定时处理子线程的数据
    void main_routine();

    /// 查找当前繁忙的子线程
    const char *who_is_busy(size_t &finished, size_t &unfinished,
                            bool skip = false);

    const std::vector<Thread *> &get_threads() const { return _threads; }

private:
    std::vector<Thread *> _threads;
};
