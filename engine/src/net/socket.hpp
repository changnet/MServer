#pragma once

#include "global/global.hpp"
#include "ev/ev_watcher.hpp"

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
    // socket的状态
    enum ConnStatus
    {
        CS_NONE    = 0, // 未指定
        CS_OPENED  = 1, // 连接已开启
        CS_CLOSING = 1, // 关闭中
        CS_CLOSED  = 2 // 已关闭
    };

public:
    ~Socket();
    explicit Socket(int32_t conn_id);

    static void library_end();
    static void library_init();

    /**
     * 把一个文件描述符设置为非阻塞(或者阻塞)
     * @param flag 0表示非阻塞，1表示阻塞
     */
    static int32_t set_block(int32_t fd, int32_t flag);
    /**
     * 启用TCP_NODELAY选项
     */
    static int32_t set_nodelay(int32_t fd);
    /// 启用TCP的keep alive
    static int32_t set_keep_alive(int32_t fd);
    /// 启用TCP的 user timeout
    static int32_t set_user_timeout(int32_t fd);
    /// 启用IPV6双栈
    static int32_t set_non_ipv6only(int32_t fd);

    /**
     * @brief 根据域名获取ip地址，此函数会阻塞
     * @param addrs ip地址数组
     * @param host 需要解析的域名
     * @return 0成功
     */
    static int32_t get_addr_info(std::vector<std::string> &addrs,
                                 const char *host, bool v4);
    /**
     * @brief 收到io回调
     * @param revents 回调的事件，如EV_ACCEPT等
    */
    void io_cb(int32_t revents);

    /// 开始接受socket数据
    bool start(int32_t fd = -1);
    /**
     * 停止socket
     * @param flush 发送缓冲区中的数据再停止
     * @param term 立即终止该socket，部分清理工作将被忽略
     */
    void stop(bool flush = false, bool term = false);
    int32_t validate();

    /**
     * 是否已关闭
     */
    bool is_closed() const { return CS_OPENED != _status; }

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

    /**
     * @brief 获取发送缓冲区
     * @return 缓冲区对象引用
    */
    Buffer &get_send_buffer()
    {
        return _w->get_send_buffer();
    }

    /**
     * @brief 获取接收缓冲区
     * @return 缓冲区对象引用
    */
    Buffer &get_recv_buffer()
    {
        return _w->get_recv_buffer();
    }

    /**
     * @brief 追加要发送的数据，但不唤醒io线程
     * @param data 需要发送的数据指针
     * @param len 需要发送的数据长度
    */
    void append(const void *data, size_t len);

    /**
     * @brief 唤醒io线程来发送数据
    */
    void flush();

    /**
     * @brief 追加要发送的数据，并且唤醒io线程
     * @param data 需要发送的数据指针
     * @param len 需要发送的数据长度
     */
    void send(const void *data, size_t len)
    {
        append(data, len);
        flush();
    }

    int32_t set_io(IO::IOType io_type, int32_t param);
    int32_t set_packet(Packet::PacketType packet_type);

    class Packet *get_packet() const { return _packet; }

    inline int32_t fd() const { return _fd; }
    inline int32_t conn_id() const { return _conn_id; }

    /**
     * @brief 设置缓冲区的参数
     * @param send_max 发送缓冲区chunk数量
     * @param recv_max 接收缓冲区chunk数量
     * @param mask 缓冲区溢出时处理方式
    */
    void set_buffer_params(int32_t send_max, int32_t recv_max, int32_t mask);

private:
    /**
     * 处理socket关闭后续工作
     * @param term 是否强制终止，部分清理工作将被忽略
     */
    void close_cb(bool term);
    void listen_cb();
    void command_cb();
    void connect_cb();
    void accept_new(int32_t fd);

protected:
    int32_t _conn_id; // 唯一id，用于回调到C++时区分连接。用fd或者指针地址，都存在复用可能会重复的问题

private:
    int8_t _status; // 当前socket的状态
    int32_t _fd; /// 当前socket的文件描述符

    EVIO *_w; /// io事件监视器
    Packet *_packet;
};
