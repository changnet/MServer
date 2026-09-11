#pragma once

#include "global/global.hpp"
#include "ev/ev_watcher.hpp"

#include "io/io.hpp"
#include "packet/packet.hpp"

class TlsCtx;
struct UdpAddr;

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
    explicit Socket(int32_t socket_id);

    static void library_end();
    static void library_init();

    /**
     * 把一个文件描述符设置为非阻塞(或者阻塞)
     * @param flag 0表示非阻塞，1表示阻塞
     */
    static int32_t set_nonblock(int32_t fd, int32_t flag);
    /**
     * 启用或者关闭TCP_NODELAY选项
     * @param opt 1启用nodelay 0关闭
     */
    int32_t set_nodelay(int32_t opt);
    /**
     * @brief 启用TCP的keep alive
     * @param time 多少秒无通信后开始发送keep alive包
     * @param interval 间隔多少秒发一次
     * @param probes 总共发多少次
     */
    int32_t set_keep_alive(int32_t time, int32_t interval, int32_t probes);
    /**
     * @brief 启用TCP的 user timeout，windows无效
     * @param timeout 单位milliseconds
     */
    int32_t set_user_timeout(int32_t timeout);
    // 启用IPV6双栈
    int32_t set_ipv6only(int32_t fd);
    /**
     * @brief 设置socket读写的事件
     * @param events 事件，例如EV_READ
     */
    int32_t set_watcher_event(int32_t events);
    /**
     * @brief 设置当前socket的版本
     * @param af_type 高16位表示AF_INET/AF_INET6，8位表示TCP/UDP，8位表示其他参数：0x01=ipv6only
     */
    void set_af_type(int32_t af_type);

    /**
     * @brief 根据域名获取ip地址，此函数会阻塞
     * @param addrs ip地址数组
     * @param host 需要解析的域名
     * @return 0成功
     */
    static int32_t get_addr_info(std::vector<std::string> &addrs,
                                 const char *host, bool v4);

    /**
     * 启动socket事件监听
     * @param addr 当前worker地址
     * @param fd 文件描述符
     * @param ev 需要监听的事件 EV_READ等
     */
    bool start(int32_t addr, int32_t fd, int32_t ev);
    /**
     * 停止socket事件监听(等待另一个线程处理完)并自动close
     * @param flush 发送缓冲区中的数据再停止
     * @return 是否成功（如果当前socket未曾调用start，则会停止失败）
     */
    void stop(bool flush = false);
    /**
     * 收到另一个线程关闭事件时，关闭socket
     * 不要在业务逻辑中调用此函数
     * @return 错误码
     */
    int32_t close();
    // 校验当前socket是否有效
    int32_t validate() const;
    // 获取当前socket的事件
    int32_t get_event();
    /**
     * @brief 设置当前socket的事件
     * @param ev 事件，例如EV_READ
     */
    void set_event(int32_t ev);
    // 获取当前的错误码
    int32_t get_errno() const;
    // 远端是否已关闭链接
    bool is_remote_close() const;

    /**
     * @brief 获取ip地址及端口
     * @return ip地址, 端口
     */
    int32_t address(lua_State *L) const;
    /**
     * @brief 开始监听链接
     * @param addr worker的地址
     * @param host 监听的ip
     * @param port 监听的端口
     * @return 文件描述符，错误返回-1
     */
    int32_t listen(int32_t addr, const char *host, int32_t port);
    /**
     * @brief 发起连接
     * @param addr worker的地址
     * @param host 连接的ip
     * @param port 连接的端口
     * @return 文件描述符，错误返回-1
     */
    int32_t connect(int32_t addr, const char *host, int32_t port);

    /**
     * 尝试接受一个fd
     * @return 成功返回fd，失败返回-1
     */
    int32_t accept(lua_State *L);
    /**
     * 获取http头数据，仅当前socket的packet为http有效
     */
    int32_t get_http_header(lua_State *L);

    /**
     * @brief 获取发送缓冲区
     * @return 缓冲区对象引用
    */
    Buffer &get_send_buffer()
    {
        return w_->io_->get_send_buffer();
    }

    /**
     * @brief 获取接收缓冲区
     * @return 缓冲区对象引用
    */
    Buffer &get_recv_buffer()
    {
        return w_->io_->get_recv_buffer();
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

    void* get_io() const
    {
        return w_ ? w_->io_ : nullptr;
    }
    /**
     * @brief 设置io读写的参数
     * @param io_type io读写方式，io或者ssl_io
     * @param ls_ctx 如果为ssl_io，需要指定ssl指针
     */
    void *set_io(int32_t io_type, TlsCtx *tls_ctx);
    /**
     * @brief 设置消息打包的类型
     * @param packet_type PT_HTTP等类型
     */
    int32_t set_packet(int32_t packet_type);

    inline int32_t fd() const { return fd_; }

    /**
     * @brief 设置缓冲区的参数
     * @param send_max 发送缓冲区大小（字节）
     * @param recv_max 接收缓冲区大小（字节）
     * @param mask 缓冲区溢出时处理方式
    */
    void set_buffer_params(int32_t send_max, int32_t recv_max, int32_t mask);

    // 初始化io
    int32_t io_init_accept();
    // 初始化io
    int32_t io_init_connect();

    // 发送原始数据
    int32_t send_pkt(lua_State *L);

    /**
     * @brief 把unpack返回的20字节地址解析成ip和port
     * @return ip地址, 端口
     */
    static int32_t get_udp_addr(lua_State *L);

    /**
     * @brief 把一条kcp连接交给backend建会话（服务端对端 / 客户端形态共用）
     * lua侧：self.s:start_kcp(worker_addr, listen_id, listen_fd, conv, addr)
     * @param worker_addr 本worker地址（backend回调投递用）
     * @param listen_id   服务端对端：监听socket_id；客户端形态：0
     * @param listen_fd   sendto用的fd（服务端对端=监听fd；客户端=自己的fd）
     * @param conv        会话号
     * @param addr        20字节的UdpAddr二进制串；客户端形态传nil（内部用默认值AF_UNSPEC）
     * @return 1 投递成功，0 失败
     */
    int32_t start_kcp(lua_State *L);

    /**
     * @brief 主线程在 start_kcp 之前预置会话号（客户端形态用，conv由客户端随机）
     */
    void set_kcp_conv(uint32_t conv)
    {
        kcp_conv_ = conv;
    }

    /**
     * @brief 把 backend 发来的 KcpAcceptMsg 拆开
     * lua侧：EngineSocket.unpack_kcp_accept(udata, usize)
     * @return listen_id, addr(20字节二进制串), conv
     */
    static int32_t unpack_kcp_accept(lua_State *L);

    // 打包前端发往后端的数据
    int32_t send_clt(lua_State *L);
    // 打包后端发往前端的数据
    int32_t send_srv(lua_State *L);
    // 发送控制帧
    int32_t send_ctrl(lua_State *L);
    /**
     * 验证connect是否成功
     * @return 成功0 失败返回非0错误码
     */
    int32_t is_connect_success();
    /**
     * 解析收到的数据，根据不同的packet类型返回不同数据
     * @return 类型编码，数据1，数据2，数据3，...
     */
    int32_t unpack(lua_State *L);
    // 在socket关闭时解析剩下的数据，仅http适用
    int32_t unpack_on_closed(lua_State *L);

private:
    int32_t fd_; /// 当前socket的文件描述符
    int32_t socket_id_; // 唯一id，用于回调到C++时区分连接。用fd或者指针地址，都存在复用可能会重复的问题
    int32_t af_type_; // 高16位表示AF_INET/AF_INET6，8位表示TCP/UDP，8位表示其他参数：0x01=ipv6only
    int32_t io_type_; // 当前的io类型，IO::IOType

    EVIO *w_; /// io事件监视器
    Packet *packet_;

    uint32_t kcp_conv_ = 0; // kcp会话号，start_kcp 之前由主线程预置
};
