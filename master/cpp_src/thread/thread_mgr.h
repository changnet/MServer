#pragma once

#include <map>
#include <pthread.h>
#include "../global/global.h"

class Thread;
class ThreadMgr
{
public:
    typedef std::map< pthread_t,class Thread * > ThreadMap;
public:
    ThreadMgr();
    ~ThreadMgr();

    void pop( pthread_t thd_id );
    void push( class Thread *thd );

    void stop();
    const char *who_is_busy(
        size_t &finished,size_t &unfinished,bool skip = false);

    const ThreadMap &get_threads() const { return _threads; }
private:
    ThreadMap _threads;
};
