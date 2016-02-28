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

class leventloop;

class socket
{
public:
    socket();
    virtual ~socket();

    static int32 non_block( int32 fd );
    static int32 keep_alive( int32 fd );
    static int32 user_timeout( int32 fd );

    void stop ();
    int32 validate();

    const char *address();
    void start( int32 fd,int32 events );
    void append( const char *data,uint32 len );
    int32 listen( const char *host,int32 port );
    int32 connect( const char *host,int32 port );

    template<class K, void (K::*method)(int32)>
    void set (K *object)
    {
        this->_this   = object;
        this->_method = &socket::method_thunk<K, method>;
    }

    inline int32 fd() const { return _w.fd; }
    inline int32 recv() { return _recv.recv(_w.fd); }
    inline int32 send() { return _send.send(_w.fd); }
    inline void set( int32 revents ) { _w.set(revents); }
    inline bool active() const { return _w.is_active(); }
    inline class buffer *get_recv_buffer() { return &_recv; }
    inline class buffer *get_send_buffer() { return &_send; }
    inline int32 accept() { return ::accept(_w.fd,NULL,NULL); }
    inline void io_cb( ev_io &w,int32 revents ) { (this->*_method)( revents ); }

    friend class leventloop;
private:
    ev_io _w;
    int32 _sending;
    buffer _recv;
    buffer _send;

    /* 采用模板类这里就可以直接保存对应类型的对象指针及成员函数，模板函数只能用void类型 */
    void *_this;
    void (socket::*_method)(int32 revents);

    template<class K, void (K::*method)(int32)>
    void method_thunk (int32 revents)
    {
      (static_cast<K *>(this->_this)->*method)(revents);
    }
};

#endif /* __SOCKET_H__ */
