#ifndef __THREAD_MGR_H__
#define __THREAD_MGR_H__

#include <map>
#include <pthread.h>
#include "../global/global.h"

class thread;
class thread_mgr
{
public:
    typedef std::map< pthread_t,class thread * > thread_mpt_t;
public:
    thread_mgr();
    ~thread_mgr();

    void pop( pthread_t thd_id );
    void push( class thread *thd );

    void stop();
    const char *who_is_busy(
        size_t &finished,size_t &unfinished,bool skip = false);

    const thread_mpt_t &get_threads() const { return _threads; }
private:
    thread_mpt_t _threads;
};

#endif /* __THREAD_MGR_H__ */
