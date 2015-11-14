#ifndef __AUTO_MUTEX_H__
#define __AUTO_MUTEX_H__

#include <pthread.h>

class auto_mutex
{
public:
    explicit auto_mutex( pthread_mutex_t *mutex )
        : _mutex( mutex )
    {
        pthread_mutex_lock( _mutex );
    }
    
    ~auto_mutex()
    {
        pthread_mutex_unlock( _mutex );
    }
private:
    pthread_mutex_t *_mutex;
};

#endif /* __AUTO_MUTEX_H__ */
