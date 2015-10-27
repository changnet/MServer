#ifndef __SOCKET_H__
#define __SOCKET_H__

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>  /* htons */

#include "../global/global.h"
#include "../ev/ev_watcher.h"
#include "buffer.h"

class socket
{
public:
    typedef enum
    {
        SK_ERROR   = 0,
        SK_SERVER  = 1,
        SK_CLIENT  = 2,
        SK_LISTEN  = 3,
        SK_HTTP    = 4
    }SOCKET_TYPE;

    ev_io w;
    int32 sending;
    buffer _recv;
    buffer _send;
    SOCKET_TYPE _type;

public:
    socket();
    virtual ~socket();
    
    inline fd()
    {
        return w.fd
    }

    inline void set_type(SOCKET_TYPE ty )
    {
        _type = ty;
    }

    static int32 non_block( int32 fd );
    static int32 keep_alive( int32 fd );
    static int32 user_timeout( int32 fd );
};

#endif /* __SOCKET_H__ */
