#pragma once

#include <map>
#include "thread.hpp"

class ThreadMgr
{
public:
    using ThreadMap = std::map<int32_t, class Thread *>;

public:
    ThreadMgr();
    ~ThreadMgr();

    void pop(pthread_t thd_id);
    void push(class Thread *thd);

    void stop();
    const char *who_is_busy(size_t &finished, size_t &unfinished,
                            bool skip = false);

    const ThreadMap &get_threads() const { return _threads; }

private:
    ThreadMap _threads;
};
