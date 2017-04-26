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

/* 网络socket连接类
 * 这里封装了基本的网络操作
 * ev_io、eventloop、getsockopt这类操作都不再给子类或外部调用
 */
class socket
{
public:
    typedef enum
    {
        CNT_NONE = 0,  // invalid connection
        CNT_CSCN = 1,  // c2s connection
        CNT_SCCN = 2,  // s2c connection
        CNT_SSCN = 3,  // s2s connection
        CNT_HTTP = 4,  // http connection

        CNT_MAXT       // max connection type
    } conn_t;
public:
    virtual ~socket();
    explicit socket( uint32 conn_id,conn_t conn_ty );

    static int32 block( int32 fd );
    static int32 non_block( int32 fd );
    static int32 keep_alive( int32 fd );
    static int32 user_timeout( int32 fd );

    virtual void command_cb () = 0;
    virtual void connect_cb () = 0;
    virtual void listen_cb  () = 0;

    void stop ();
    void start( int32 fd = 0);
    int32 validate();

    const char *address();
    int32 listen( const char *host,int32 port );
    int32 connect( const char *host,int32 port );

    bool __attribute__ ((warn_unused_result))
        append( const char *data,uint32 len ) ;

    template<class K, void (K::*method)()>
    void set (K *object)
    {
        this->_this   = object;
        this->_method = &socket::method_thunk<K, method>;
    }

    inline int32 recv()
    {
        if ( !_recv.reserved() ) return -1; /* no more memory */

        assert( "socket recv buffer length <= 0",_recv._len - _recv._size > 0 );
        int32 len = ::read( 
            _w.fd,_recv._buff + _recv._size,_recv._len - _recv._size );
        if ( len > 0 )
        {
            _recv._size += len;
        }
        return len;
    }

    inline int32 send()
    {
        assert( "buf send without data",_send._size - _send._pos > 0 );

        int32 len = ::write( 
            _w.fd,_send._buff + _send._pos,_send._size - _send._pos );
        if ( len > 0 )
        {
            _send._pos += len;
        }

        return len;
    }

    inline int32 fd() const { return _w.fd; }
    inline uint32 conn_id() { return _conn_id; }
    inline conn_t conn_type() { return _conn_ty; }
    inline bool active() const { return _w.is_active(); }
    inline int32 accept() { return ::accept(_w.fd,NULL,NULL); }
    inline void io_cb( ev_io &w,int32 revents ) { (this->*_method)(); }

    friend class leventloop;
protected:
    buffer _recv;
    buffer _send;
    int32  _sending;
    uint32 _conn_id;
    conn_t _conn_ty;

    void pending_send();
private:
    ev_io _w;

    /* 采用模板类这里就可以直接保存对应类型的对象指针及成员函数，模板函数只能用void类型 */
    void *_this;
    void (socket::*_method)();

    template<class K, void (K::*method)()>
    void method_thunk ()
    {
      (static_cast<K *>(this->_this)->*method)();
    }
};

#endif /* __SOCKET_H__ */
