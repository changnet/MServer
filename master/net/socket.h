#ifndef __SOCKET_H__
#define __SOCKET_H__

#include "../global/global.h"
#include "../ev/ev_watcher.h"
#include "buffer.h"

class socket
{
public:
    ev_io w;
    buffer _recv;
    buffer _send;
public:
    inline void stop()
    {
        ::close( w.fd );
        w.stop();
        /* TODO buff deallocate */
    }
};

#endif /* __SOCKET_H__ */
