#ifndef __THREAD_MGR_H__
#define __THREAD_MGR_H__

#include <map>
#include <pthread.h>
#include "../global/global.h"


class thread;
class thread_mgr
{
public:
    static thread_mgr *instance();
    static void uninstance();
    
    void push( class thread *_thread );
    class thread *pop( pthread_t _thread_t );
    
    void clear();
private:
    thread_mgr();
    ~thread_mgr();

    std::map< pthread_t,class thread * > _threads;
    static class thread_mgr *_instance;
};

#endif /* __THREAD_MGR_H__ */
