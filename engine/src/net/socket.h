#pragma once

#include "../ev/ev_watcher.h"
#include "../global/global.h"

#include "buffer.h"
#include "codec/codec.h"
#include "io/io.h"
#include "packet/packet.h"

#ifdef TCP_KEEP_ALIVE
    #define KEEP_ALIVE(x) Socket::keep_alive(x)
#else
    #define KEEP_ALIVE(x)
#endif

#ifdef _TCP_USER_TIMEOUT
    #define USER_TIMEOUT(x) Socket::user_timeout(x)
#else
    #define USER_TIMEOUT(x)
#endif

class LEV;

/* 网络socket连接类
 * 这里封装了基本的网络操作
 * ev_io、eventloop、getsockopt这类操作都不再给子类或外部调用
 */
class Socket
{
public:
    typedef enum
    {
        CT_NONE = 0, ///< 连接方式，无效值
        CT_CSCN = 1, ///< c2s 客户端与服务器之间连接
        CT_SCCN = 2, ///< s2c 服务器与客户端之间连接
        CT_SSCN = 3, ///< s2s 服务器与服务器之间连接

        CT_MAX ///< 连接方式最大值
    } ConnType;

    // socket缓冲区溢出后处理方式
    typedef enum
    {
        OAT_NONE = 0, // 不做任何处理
        OAT_KILL = 1, // 断开连接，通常用于与客户端连接
        OAT_PEND = 2, // 阻塞，通用用于服务器之间连接

        OAT_MAX
    } OverActionType;

public:
    virtual ~Socket();
    explicit Socket(uint32_t conn_id, ConnType conn_ty);

    static int32_t block(int32_t fd);
    static int32_t non_block(int32_t fd);
    static int32_t keep_alive(int32_t fd);
    static int32_t user_timeout(int32_t fd);

    void listen_cb();
    void command_cb();
    void connect_cb();

    int32_t recv();
    int32_t send();

    void start(int32_t fd = 0);
    void stop(bool flush = false);
    int32_t validate();
    void pending_send();

    const char *address();
    int32_t listen(const char *host, int32_t port);
    int32_t connect(const char *host, int32_t port);

    int32_t init_accept();
    int32_t init_connect();

    inline void append(const void *data, uint32_t len)
    {
        _send.append(data, len);

        pending_send();
    }

    template <class K, void (K::*method)()> void set(K *object)
    {
        this->_this   = object;
        this->_method = &Socket::method_thunk<K, method>;
    }

    int32_t set_io(IO::IOT io_type, int32_t param);
    int32_t set_packet(Packet::PacketType packet_type);
    int32_t set_codec_type(Codec::CodecType codec_type);

    class Packet *get_packet() const { return _packet; }
    Codec::CodecType get_codec_type() const { return _codec_ty; }

    inline int32_t fd() const { return _w.fd; }
    inline uint32_t conn_id() const { return _conn_id; }
    inline ConnType conn_type() const { return _conn_ty; }
    inline bool active() const { return _w.is_active(); }
    inline class Buffer &recv_buffer() { return _recv; }
    inline class Buffer &send_buffer() { return _send; }
    inline void io_cb(EVIO &w, int32_t revents) { (this->*_method)(); }

    inline int32_t get_pending() const { return _pending; }
    inline int32_t set_pending(int32_t pending) { return _pending = pending; }

    inline void set_recv_size(uint32_t max, uint32_t ctx_size)
    {
        _recv.set_buffer_size(max, ctx_size);
    }
    inline void set_send_size(uint32_t max, uint32_t ctx_size, OverActionType oa)
    {
        _over_action = oa;
        _send.set_buffer_size(max, ctx_size);
    }

    inline int64_t get_object_id() const { return _object_id; }
    inline void set_object_id(int64_t oid) { _object_id = oid; }

    /* 获取统计数据
     * @schunk:发送缓冲区分配的内存块
     * @rchunk:接收缓冲区分配的内存块
     * @smem:发送缓冲区分配的内存大小
     * @rmem:接收缓冲区分配的内在大小
     * @spending:待发送的数据
     * @rpending:待处理的数据
     */
    void get_stat(uint32_t &schunk, uint32_t &rchunk, uint32_t &smem,
                  uint32_t &rmem, uint32_t &spending, uint32_t &rpending);

private:
    // 检查io返回值: < 0 错误，0 成功，1 需要重读，2 需要重写
    int32_t io_status_check(int32_t ecode);

protected:
    Buffer _recv;
    Buffer _send;
    int32_t _pending;
    uint32_t _conn_id;
    ConnType _conn_ty;

private:
    EVIO _w;
    int64_t _object_id; /* 标识这个socket对应上层逻辑的object，一般是玩家id */

    class IO *_io;
    class Packet *_packet;
    Codec::CodecType _codec_ty;
    OverActionType _over_action;

    /* 采用模板类这里就可以直接保存对应类型的对象指针及成员函数，模板函数只能用void类型
     */
    void *_this;
    void (Socket::*_method)();

    template <class K, void (K::*method)()> void method_thunk()
    {
        (static_cast<K *>(this->_this)->*method)();
    }
};
