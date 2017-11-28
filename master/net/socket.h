#ifndef __SOCKET_H__
#define __SOCKET_H__

#include "../global/global.h"
#include "../ev/ev_watcher.h"

#include "buffer.h"
#include "io/io.h"
#include "codec/codec.h"
#include "packet/packet.h"

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

    void listen_cb ();
    void command_cb();
    void connect_cb();

    int32 recv();
    int32 send();

    void stop ();
    void start( int32 fd = 0);
    int32 validate();
    void pending_send();

    const char *address();
    int32 listen( const char *host,int32 port );
    int32 connect( const char *host,int32 port );

    bool __attribute__ (
        (warn_unused_result)) append( const void *data,uint32 len );

    template<class K, void (K::*method)()>
    void set (K *object)
    {
        this->_this   = object;
        this->_method = &socket::method_thunk<K, method>;
    }

    int32 set_io( io::io_t io_type );
    int32 set_packet( packet::packet_t packet_type );
    int32 set_codec_type( codec::codec_t codec_type );

    class packet *get_packet() const { return _packet; }
    codec::codec_t get_codec_type() const { return _codec_ty; }

    inline int32 fd() const { return _w.fd; }
    inline uint32 conn_id() const { return _conn_id; }
    inline conn_t conn_type() const { return _conn_ty; }
    inline bool active() const { return _w.is_active(); }
    inline class buffer &recv_buffer() { return _recv; }
    inline class buffer &send_buffer() { return _send; }
    inline void io_cb( ev_io &w,int32 revents ) { (this->*_method)(); }

    inline void set_recv_size( uint32 max,uint32 min )
    {
        _recv.set_buffer_size( max,min );
    }
    inline void set_send_size( uint32 max,uint32 min )
    {
        _send.set_buffer_size( max,min );
    }

    friend class leventloop;
protected:
    buffer _recv;
    buffer _send;
    int32  _sending;
    uint32 _conn_id;
    conn_t _conn_ty;
private:
    ev_io _w;

    class io *_io;
    class packet *_packet;
    codec::codec_t _codec_ty;

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
