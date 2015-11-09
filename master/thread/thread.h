#ifndef __THREAD_H__
#define __THREAD_H__

#include <pthread.h>
#include "../global/global.h"

class thread
{
public:
    thread();
    virtual ~thread();
    
    bool start();
    void stop ();
    
    pthread_t get_id();
    void join();
protected:
    virtual void routine() = 0;
    static void *start_routine( void *arg );
protected:
    volatile bool _run;
    volatile bool _join;
    pthread_t thread_t;
    pthread_mutex_t mutex;
};

#endif /* __THREAD_H__ */
