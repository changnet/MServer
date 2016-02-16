#ifndef __SOCKET_H__
#define __SOCKET_H__

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>  /* htons */

#include "../global/global.h"
#include "../ev/ev_watcher.h"
#include "buffer.h"

#ifdef TCP_KEEP_ALIVE
# define KEEP_ALIVE(x)    socket::keep_alive(x)
#else
# define KEEP_ALIVE(x)
#endif

#ifdef _TCP_USER_TIMEOUT
# define USER_TIMEOUT(x)    socket::user_timeout(x)
#else
# define USER_TIMEOUT(x)
#endif

class socket
{
public:
    socket();
    virtual ~socket();

    inline bool active() const { return _w.is_active(); }
    inline int32 fd() const { return _w.fd; }

    static int32 non_block( int32 fd );
    static int32 keep_alive( int32 fd );
    static int32 user_timeout( int32 fd );
    
    void stop ();
    void start( int32 fd,int32 events );
    inline void io_cb( ev_io &w,int32 revents ) { this->_method( revents ); }
    
    template<class K, void (K::*method)(int32)>
    void set (K *object)
    {
        this->_this   = object;
        this->_method = method_thunk<K, method>;
    }
private:
    ev_io _w;
    int32 _sending;
    buffer _recv;
    buffer _send;
    
    /* 采用模板类这里就可以直接保存对应类型的对象指针及成员函数，模板函数只能用void类型 */
    void *_this;
    void (*_method)(int32 revents);
    
    template<class K, void (K::*method)(int32)>
    void method_thunk (int32 revents)
    {
      (static_cast<K *>(this->_this)->*method)(revents);
    }
};

#endif /* __SOCKET_H__ */
