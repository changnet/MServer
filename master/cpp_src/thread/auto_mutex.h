#pragma once

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
