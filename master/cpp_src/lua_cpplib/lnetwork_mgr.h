#pragma once

#include <vector>

#include "../net/net_header.h"
#include "../net/socket.h"

struct lua_State;
class LNetworkMgr
{
private:
    typedef StdMap<int32_t,CmdCfg> cmd_map_t;
    typedef StdMap<uint32_t,class Socket*> socket_map_t;
public:
    ~LNetworkMgr();
    explicit LNetworkMgr();

    /* 设置指令参数 */
    int32_t set_cs_cmd( lua_State *L );
    int32_t set_ss_cmd( lua_State *L );
    int32_t set_sc_cmd( lua_State *L );

    int32_t set_conn_io    ( lua_State *L ); /* 设置socket的io方式 */
    int32_t set_conn_codec ( lua_State *L ); /* 设置socket的编译方式 */
    int32_t set_conn_packet( lua_State *L ); /* 设置socket的打包方式 */

    int32_t set_conn_owner  ( lua_State *L ); /* 设置(客户端)连接所有者 */
    int32_t unset_conn_owner( lua_State *L ); /* 解除(客户端)连接所有者 */
    int32_t set_conn_session( lua_State *L ); /* 设置(服务器)连接session */
    int32_t set_curr_session( lua_State *L ); /* 设置当前服务器的session */

    int32_t load_one_schema( lua_State *L ); /* 加载schema文件 */
    int32_t get_http_header( lua_State *L ); /* 获取http报文头数据 */

    /* 这三个是通用接口，数据打包差异是通过packet的多态实现的 */
    int32_t send_srv_packet( lua_State *L ); /* 发送往服务器数据包 */
    int32_t send_clt_packet( lua_State *L ); /* 发送往客户端数据包 */
    int32_t send_raw_packet( lua_State *L ); /* 发送原始的数据包 */

    /* 这些是在同一种packet上发送不同特定数据包 */
    int32_t send_s2s_packet ( lua_State *L ); /* 发送s2s数据包 */
    int32_t send_ssc_packet ( lua_State *L ); /* 跨服务器发送客户端数据包 */
    int32_t send_rpc_packet ( lua_State *L ); /* 发送rpc数据包 */
    int32_t send_ctrl_packet( lua_State *L ); /* 发送ping-pong等数据包 */

    /* 组播数据包 */
    int32_t srv_multicast( lua_State *L ); /* 广播到所有连接到当前进程的服务器 */
    int32_t clt_multicast( lua_State *L ); /* 网关进程广播数据到客户端 */
    int32_t ssc_multicast( lua_State *L ); /* 非网关数据广播数据到客户端 */

    int32_t set_send_buffer_size( lua_State *L ); /* 设置发送缓冲区大小 */
    int32_t set_recv_buffer_size( lua_State *L ); /* 设置接收缓冲区大小 */

    int32_t new_ssl_ctx( lua_State *L ); /* 创建一个ssl上下文 */

    // 设置、获取玩家当前所在的session
    int32_t set_player_session( lua_State *L );
    int32_t get_player_session( lua_State *L );

    /* socket基本操作 */
    int32_t close   ( lua_State *L );
    int32_t address ( lua_State *L );
    int32_t listen  ( lua_State *L );
    int32_t connect ( lua_State *L );

public:
    void clear(); /* 清除所有网络数据，不通知上层脚本 */

    /* 删除无效的连接 */
    void invoke_delete();

    /* 通过所有者查找连接id */
    uint32_t get_conn_id_by_owner( Owner owner ) const;

    /* 通过session获取socket连接 */
    class Socket *get_conn_by_session( int32_t session ) const;

    /* 通过onwer获取socket连接 */
    class Socket *get_conn_by_owner( Owner owner ) const;

    /* 通过conn_id获取socket连接 */
    class Socket *get_conn_by_conn_id( uint32_t conn_id ) const;

    /* 通过conn_id获取session */
    int32_t get_session_by_conn_id( uint32_t conn_id ) const;

    /* 获取指令配置 */
    const CmdCfg *get_cs_cmd( int32_t cmd ) const;
    const CmdCfg *get_ss_cmd( int32_t cmd ) const;
    const CmdCfg *get_sc_cmd( int32_t cmd ) const;
    /* 获取当前服务器session */
    int32_t get_curr_session() const { return _session; }
    uint32_t new_connect_id(); /* 获取新connect_id */

    bool connect_del( uint32_t conn_id );
    bool connect_new( uint32_t conn_id,int32_t ecode );
    bool accept_new ( uint32_t conn_id,class Socket *new_sk );

    /* 自动转发 */
    bool cs_dispatch( int32_t cmd,
        const class Socket *src_sk,const char *ctx,size_t size ) const;
private:
    void delete_socket( uint32_t conn_id );
    int32_t get_cmd_session( int64_t object_id,int32_t cmd ) const;
    class Packet *lua_check_packet( lua_State *L,Socket::ConnType conn_ty );
    class Packet *raw_check_packet(
        lua_State *L,uint32_t conn_id,Socket::ConnType conn_ty );
private:
    int32_t _session; /* 当前进程的session */
    uint32_t _conn_seed; /* connect_id种子 */

    cmd_map_t _cs_cmd_map;
    cmd_map_t _ss_cmd_map;
    cmd_map_t _sc_cmd_map;
    socket_map_t _socket_map;

    std::vector<uint32_t> _deleting;/* 异步删除的socket */
    /* owner-conn_id 映射,ssc数据包转发时需要 */
    StdMap<Owner,uint32_t> _owner_map;
    StdMap<Owner,int32_t> _owner_session; /* owner-session映射 */

    StdMap<int32_t,uint32_t> _session_map; /* session-conn_id 映射 */
    StdMap<uint32_t,Owner> _conn_session_map; /* conn_id-session 映射 */
};
