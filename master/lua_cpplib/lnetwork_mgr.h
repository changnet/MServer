#ifndef __LNETWORK_MGR_H__
#define __LNETWORK_MGR_H__

#include "../net/net_include.h"
#include "../net/socket.h"

#include <vector>
#if __cplusplus < 201103L    /* -std=gnu99 */
    #include <map>
    #define map_t    std::map
#else                       /* if support C++ 2011 */
    #include <unordered_map>
    #define map_t    std::unordered_map
#endif

struct lua_State;
class lnetwork_mgr
{
private:
    typedef map_t<int32,cmd_cfg_t> cmd_map_t;
    typedef map_t<uint32,class socket*> socket_map_t;
public:
    /* 设置指令参数 */
    int32 set_cs_cmd();
    int32 set_ss_cmd();
    int32 set_sc_cmd();

    int32 set_conn_io(); /* 设置socket的io方式 */
    int32 set_conn_codec (); /* 设置socket的编译方式 */
    int32 set_conn_packet(); /* 设置socket的打包方式 */

    int32 set_conn_owner  (); /* 设置(客户端)连接所有者 */
    int32 set_conn_session(); /* 设置(服务器)连接session */
    int32 set_curr_session(); /* 设置当前服务器的session */

    int32 load_one_schema(); /* 加载schema文件 */
    int32 get_http_header(); /* 获取http报文头数据 */

    int32 send_c2s_packet(); /* 发送c2s数据包 */
    int32 send_s2c_packet(); /* 发送s2c数据包 */
    int32 send_s2s_packet(); /* 发送s2s数据包 */
    int32 send_ssc_packet(); /* 跨服务器发送客户端数据包 */
    int32 send_rpc_packet(); /* 发送rpc数据包 */
    int32 send_raw_packet(); /* 发送原始的数据包 */
    int32 send_webs_srv_packet();/* 发送websocket数据包 */
    int32 send_webs_clt_packet();/* 发送websocket数据包 */

    int32 set_send_buffer_size(); /* 设置发送缓冲区大小 */
    int32 set_recv_buffer_size(); /* 设置接收缓冲区大小 */

    /* socket基本操作 */
    int32 close();
    int32 address ();
    int32 listen  ();
    int32 connect ();

public:
    static void uninstance();
    static class lnetwork_mgr *instance();

    ~lnetwork_mgr();
    explicit lnetwork_mgr( lua_State *L );

    /* 删除无效的连接 */
    void invoke_delete();

    /* 通过连接id查找所有者 */
    owner_t get_owner_by_conn_id( uint32 conn_id ) const;
    /* 通过所有者查找连接id */
    uint32 get_conn_id_by_owner( owner_t owner ) const;

    /* 通过session获取socket连接 */
    class socket *get_conn_by_session( int32 session ) const;

    /* 通过onwer获取socket连接 */
    class socket *get_conn_by_owner( owner_t owner ) const;

    /* 通过conn_id获取socket连接 */
    class socket *get_conn_by_conn_id( uint32 conn_id ) const;

    /* 通过conn_id获取session */
    int32 get_session_by_conn_id( uint32 conn_id ) const;

    /* 获取指令配置 */
    const cmd_cfg_t *get_cs_cmd( int32 cmd ) const;
    const cmd_cfg_t *get_ss_cmd( int32 cmd ) const;
    const cmd_cfg_t *get_sc_cmd( int32 cmd ) const;
    /* 获取当前服务器session */
    int32 get_curr_session() const { return _session; }

    class socket *accept_new( socket::conn_t conn_ty );
    bool connect_new( uint32 conn_id,int32 conn_ty,int32 ecode );
    bool connect_del( uint32 conn_id,int32 conn_ty );
private:
    uint32 new_connect_id(); /* 获取新connect_id */
    void delete_socket( uint32 conn_id );
    class packet *lua_check_packet( socket::conn_t conn_ty );
private:
    struct lua_State *L;

    int32 _session; /* 当前进程的session */
    uint32 _conn_seed; /* connect_id种子 */

    cmd_map_t _cs_cmd_map;
    cmd_map_t _ss_cmd_map;
    cmd_map_t _sc_cmd_map;
    socket_map_t _socket_map;

    std::vector<uint32> _deleting;/* 异步删除的socket */
    /* owner-conn_id 映射,ssc数据包转发时需要 */
    map_t<owner_t,uint32> _owner_map;
    /* conn_id-owner 映射，css数据包转发时需要 */
    map_t<uint32,owner_t> _conn_owner_map;

    map_t<int32,uint32> _session_map; /* session-conn_id 映射 */
    map_t<uint32,owner_t> _conn_session_map; /* conn_id-session 映射 */
    static class lnetwork_mgr *_network_mgr;
};

#endif /* __LNETWORK_MGR_H__ */