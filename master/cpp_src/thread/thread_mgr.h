#pragma once

#include <map>
#include <pthread.h>
#include "../global/global.h"

class Thread;
class ThreadMgr
{
public:
    typedef std::map< pthread_t,class Thread * > thread_mpt_t;
public:
    ThreadMgr();
    ~ThreadMgr();

    void pop( pthread_t thd_id );
    void push( class Thread *thd );

    void stop();
    const char *who_is_busy(
        size_t &finished,size_t &unfinished,bool skip = false);

    const thread_mpt_t &get_threads() const { return _threads; }
private:
    thread_mpt_t _threads;
};
