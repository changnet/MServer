#pragma once

#include <vector>

#include "../net/net_header.hpp"
#include "../net/socket.hpp"

struct lua_State;

/**
 * 网络管理
 */
class LNetworkMgr
{
private:
    using socket_map_t = std::unordered_map<int32_t, class Socket *>;

public:
    ~LNetworkMgr();
    explicit LNetworkMgr();

    /**
     * 设置连接的io方式
     * @param conn_id 连接id
     * @param io_type io类型，见io.h中的定义，1表示ssl
     * @param io_ctx io内容，根据不同io类型意义不一样，ssl则表示ssl_mgr中的索引
     */
    int32_t set_conn_io(lua_State *L);

    /**
     * 设置socket的打包方式
     * @param conn_id 连接id
     * @param packet_type 打包类型，见packet.h中的定义(http、tcp、websocket...)
     */
    int32_t set_conn_packet(lua_State *L);

    /**
     * 设置(客户端)连接所有者
     * @param conn_id 连接id
     * @param owner 所有者id，一般是玩家id，用于自动转发
     */
    int32_t set_conn_owner(lua_State *L);

    /**
     * 解除(客户端)连接所有者
     * @param conn_id 连接id
     * @param owner 所有者id，一般是玩家id(TODO: 这个参数应该可以精简掉)
     */
    int32_t unset_conn_owner(lua_State *L); /* 解除(客户端)连接所有者 */

    /**
     * 设置(服务器)连接session，用于标记该连接是和哪个进程连接的
     * @param conn_id 连接id
     * @param session 会话id
     */
    int32_t set_conn_session(lua_State *L);

    /**
     * 设置当前服务器的session
     * @param conn_id 连接id
     * @param session 当前服务器会话id
     */
    int32_t set_curr_session(lua_State *L);

    /**
     * 获取http报文头数据
     * @param conn_id 连接id
     * @return upgrade code method fields
     * upgrade 是否为websocket
     * code http错误码
     * method http方式 GET POST
     * fields 报文各个字段(lua table,kv格式)
     */
    int32_t get_http_header(lua_State *L);

    /**
     * 客户端向服务器发包
     * @param conn_id 连接id
     * @param cmd 协议号
     * @param pkt 数据包(lua table)
     */
    int32_t send_srv_packet(lua_State *L); /* 发送往服务器数据包 */

    /**
     * 服务器向客户端发包
     * @param conn_id 连接id
     * @param cmd 协议号
     * @param errno 错误码
     * @param pkt 数据包(lua table)
     */
    int32_t send_clt_packet(lua_State *L); /* 发送往客户端数据包 */

    /**
     * 发送原始的数据包(该数据不会经过任何编码、打包，直接发送出去)
     * @param conn_id 连接id
     * @param content 数据(string)
     */
    int32_t send_raw_packet(lua_State *L); /* 发送原始的数据包 */

    /**
     * 服务器向服务器发包
     * @param conn_id 连接id
     * @param cmd 协议号
     * @param errno 错误码
     * @param pkt 数据包(lua table)
     */
    int32_t send_s2s_packet(lua_State *L);

    /**
     * 在非网关发送客户端数据包，该包会由网关直接转发给客户端
     * @param conn_id 连接id
     * @param cmd 协议号
     * @param errno 错误码
     * @param pkt 数据包(lua table)
     */
    int32_t send_ssc_packet(lua_State *L);

    /**
     * 发送rpc请求
     * @param conn_id 连接id
     * @param unique_id 唯一id，用于返回值回调
     * @param name 函数名
     * @param ... 可变参数
     */
    int32_t send_rpc_packet(lua_State *L);

    /**
     * 发送ping-pong等控制数据包，仅连接为websocket时有效
     * @param conn_id 连接id
     * @param mask 控制掩码，如WS_OP_PING
     * @param ... 数据，根据不同的掩码不一样，可能是字符串或其他数据
     */
    int32_t send_ctrl_packet(lua_State *L);

    /**
     * 非网关数据广播数据到客户端
     * @param conn_id 网关连接id
     * @param mask 掩码，见net_header.h clt_multicast_t定义
     * @param arg_list, 参数列表，如果掩码是根据连接广播，则该列表为连接id
     *                  如果根据玩家id广播，则为玩家id
     * @param codec_type 编码方式(protobuf、flatbuffers)
     * @param cmd 协议号
     * @param errno 错误码
     * @param pkt 数据包(lua table)
     */
    int32_t ssc_multicast(lua_State *L); /* 非网关数据广播数据到客户端 */

    /**
     * 设置收发缓冲区参数
     * @param conn_id 网关连接id
     * @param send_chunk_max 发送缓冲区chunk数量
     * @param recv_chunk_max 接收缓冲区chunk数量
     * @param action 缓冲区溢出处理方式，见socket.h中OverActionType定义(1断开，2阻塞)
     */
    int32_t set_buffer_params(lua_State *L);

    /**
     * 创建一个ssl上下文
     * @param sslv ssl版本，见ssl_mgr.h中的定义
     * @param cert_file 可选参数,服务端认证文件路径(通常不开启服务端认证)
     * @param key_type 服务端密钥类型(pem文件的加密类型，如RSA，见ssl_mgr.h中定义)
     * @param key_file 服务端密钥文件(pem文件路径)
     * @param password 密钥文件的密码
     */
    int32_t new_ssl_ctx(lua_State *L); /* 创建一个ssl上下文 */

    /**
     * 设置玩家当前所在的session(比如玩家在不同场景进程中切换时，需要同步玩家数据到场景)
     * @param pid 玩家id
     * @param session 会话id
     */
    int32_t set_player_session(lua_State *L);

    /**
     * 设置、获取玩家当前所在的session
     * @param pid 玩家id
     */
    int32_t get_player_session(lua_State *L);

    /**
     * 关闭socket
     * @param conn_id 网关连接id
     * @param flush 是否发送还在缓冲区的数据
     */
    int32_t close(lua_State *L);

    /**
     * 获取某个连接的ip地址
     * @param conn_id 网关连接id
     */
    int32_t address(lua_State *L);

    /**
     * 监听连接
     * @param host 监听的主机，0.0.0.0或127.0.0.1
     * @param port 监听端口
     * @param conn_type 连接类型（cs、sc、ss），见socket.h中的定义
     * @return fd, message，成功只返回fd；失败fd为-1，则message为错误原因
     */
    int32_t listen(lua_State *L);

    /**
     * 主动连接其他服务器
     * @param host 目标主机
     * @param port 目标端口
     * @param conn_type 连接类型（cs、sc、ss），见socket.h中的定义
     */
    int32_t connect(lua_State *L);

    /**
     * 获取连接类型
     * @param conn_id 网关连接id
     * @return 连接类型，参考 ConnType 定义
     */
    int32_t get_connect_type(lua_State *L);

public:
    /// 清除所有网络数据，不通知上层脚本
    void clear();

    /// 删除无效的连接
    void invoke_delete();

    /// 通过所有者查找连接id
    int32_t get_conn_id_by_owner(Owner owner) const;

    /// 通过session获取socket连接
    class Socket *get_conn_by_session(int32_t session) const;

    /// 通过onwer获取socket连接
    class Socket *get_conn_by_owner(Owner owner) const;

    /// 通过conn_id获取socket连接
    class Socket *get_conn_by_conn_id(int32_t conn_id) const;

    /// 通过conn_id获取session
    int32_t get_session_by_conn_id(int32_t conn_id) const;

    /// 获取当前服务器session
    int32_t get_curr_session() const
    {
        return _session;
    }

    /* 产生一个唯一的连接id
     * 之所以不用系统的文件描述符fd，是因为fd对于上层逻辑不可控。比如一个fd被释放，可能在多个进程
     * 之间还未处理完，此fd就被重用了。当前的连接id不太可能会在短时间内重用。
     */
    int32_t new_connect_id();

    /// io建立完成
    bool io_ok(int32_t conn_id);
    /// 连接被销毁（断开）
    bool connect_del(int32_t conn_id, int32_t e);
    /// 连接成功，需要执行始化
    bool connect_new(int32_t conn_id, int32_t ecode);
    /// 接受连接成功，需要执行始化
    bool accept_new(int32_t conn_id, class Socket *new_sk);

private:
    void delete_socket(int32_t conn_id);
    class Packet *lua_check_packet(lua_State *L, Socket::ConnType conn_ty);
    class Packet *raw_check_packet(lua_State *L, int32_t conn_id,
                                   Socket::ConnType conn_ty);

private:
    int32_t _session;   /* 当前进程的session */
    int32_t _conn_seed; /* connect_id种子 */

    socket_map_t _socket_map;

    /// 异步删除的socket
    std::unordered_map<int32_t, int32_t> _deleting;

    /// owner-conn_id 映射,ssc数据包转发时需要
    std::unordered_map<Owner, int32_t> _owner_map;

    /// owner-session映射
    std::unordered_map<Owner, int32_t> _owner_session;

    std::unordered_map<int32_t, int32_t> _session_map; /* session-conn_id 映射 */
    std::unordered_map<int32_t, Owner> _conn_session_map; /* conn_id-session 映射 */
};
