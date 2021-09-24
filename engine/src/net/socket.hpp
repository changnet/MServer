#pragma once

#include "../ev/ev_watcher.hpp"
#include "../global/global.hpp"

#include "buffer.hpp"
#include "codec/codec.hpp"
#include "io/io.hpp"
#include "packet/packet.hpp"

class LEV;

/* 网络socket连接类
 * 这里封装了基本的网络操作
 * ev_io、eventloop、getsockopt这类操作都不再给子类或外部调用
 */
class Socket final
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
    ~Socket();
    explicit Socket(uint32_t conn_id, ConnType conn_ty);

    static void library_end();
    static void library_init();

    /// 把一个文件描述符设置为阻塞
    static int32_t block(int32_t fd);
    /// 把一个文件描述符设置为非阻塞
    static int32_t non_block(int32_t fd);
    /// 启用TCP的keep alive
    static int32_t keep_alive(int32_t fd);
    /// 启用TCP的 user timeout
    static int32_t user_timeout(int32_t fd);
    /// 启用IPV6双栈
    static int32_t non_ipv6only(int32_t fd);

    /**
     * @brief 根据域名获取ip地址，此函数会阻塞
     * @param addrs ip地址数组
     * @param host 需要解析的域名
     * @return 0成功
     */
    static int32_t get_addr_info(std::vector<std::string> &addrs,
                                 const char *host);

    void listen_cb(int32_t revents);
    void command_cb(int32_t revents);
    void connect_cb(int32_t revents);

    int32_t recv();
    int32_t send();

    /// 开始接受socket数据
    void start(int32_t fd = -1);
    void stop(bool flush = false);
    int32_t validate();
    void pending_send();

    /// 是否已关闭
    bool is_closed() const { return !fd_valid(fd()); }

    /**
     * 获取ip地址及端口
     * @brief address
     * @param buf
     * @param len must be at least INET6_ADDRSTRLEN bytes long
     * @param port
     * @return
     */
    const char *address(char *buf, size_t len, int *port);
    int32_t listen(const char *host, int32_t port);
    int32_t connect(const char *host, int32_t port);

    int32_t init_accept();
    int32_t init_connect();

    /// 添加待发送数据
    inline void append(const void *data, size_t len)
    {
        _send.append(data, len);

        pending_send();
    }

    int32_t set_io(IO::IOT io_type, int32_t param);
    int32_t set_packet(Packet::PacketType packet_type);
    int32_t set_codec_type(Codec::CodecType codec_type);

    class Packet *get_packet() const { return _packet; }
    Codec::CodecType get_codec_type() const { return _codec_ty; }

    inline int32_t fd() const { return _w.get_fd(); }
    inline uint32_t conn_id() const { return _conn_id; }
    inline ConnType conn_type() const { return _conn_ty; }
    inline bool active() const { return _w.active(); }
    inline class Buffer &recv_buffer() { return _recv; }
    inline class Buffer &send_buffer() { return _send; }

    inline int32_t get_pending() const { return _pending; }
    inline int32_t set_pending(int32_t pending) { return _pending = pending; }

    inline void set_recv_size(size_t max, size_t ctx_size)
    {
        _recv.set_buffer_size(max, ctx_size);
    }
    inline void set_send_size(size_t max, size_t ctx_size, OverActionType oa)
    {
        _over_action = oa;
        _send.set_buffer_size(max, ctx_size);
    }

    inline int64_t get_object_id() const { return _object_id; }
    inline void set_object_id(int64_t oid) { _object_id = oid; }

    /**
     * 获取统计数据
     * @param schunk 发送缓冲区分配的内存块
     * @param rchunk 接收缓冲区分配的内存块
     * @param smem 发送缓冲区分配的内存大小
     * @param rmem 接收缓冲区分配的内在大小
     * @param spending 待发送的数据
     * @param rpending 待处理的数据
     */
    void get_stat(size_t &schunk, size_t &rchunk, size_t &smem,
                  size_t &rmem, size_t &spending, size_t &rpending);

    /// 获取socket错误日志
    static const char *str_error(int32_t e = -1);
    /// 获取无效的socket id
    static inline int32_t invalid_fd()
    {
#ifdef __windows__
        return INVALID_SOCKET;
#else
        return -1;
#endif
    }
    /// 判断创建出来的fd是否有效
    static bool fd_valid(int32_t fd);

    /// 获取socket的错误码
    static int32_t error_no();
    /// 是否真正出现了错误
    static int32_t is_error();

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
    OverActionType _over_action; /// 缓冲区溢出时，采取的措施
};
