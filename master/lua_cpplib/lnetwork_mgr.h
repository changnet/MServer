#ifndef __LNETWORK_MGR_H__
#define __LNETWORK_MGR_H__

#include <lua.hpp>

#include "../net/packet.h"
#include "../net/socket.h"
#include "../global/global.h"

#include <vector>
#if __cplusplus < 201103L    /* -std=gnu99 */
    #include <map>
    #define map_t    std::map
#else                       /* if support C++ 2011 */
    #include <unordered_map>
    #define map_t    std::unordered_map
#endif

#define MAX_SCHEMA_NAME  64

/* _maks 设定掩码，按位
 * 1bit: 解码方式:0 普通解码，1 unpack解码
 * 2bit: TODO:广播方式 ？？这个放到脚本
 */
struct cmd_cfg_t
{
    int32 _cmd;
    int32 _mask;
    int32 _session;
    char _schema[MAX_SCHEMA_NAME];
    char _object[MAX_SCHEMA_NAME];
};

class lnetwork_mgr
{
private:
    typedef map_t<int32,cmd_cfg_t> cmd_map_t;
    typedef map_t<uint32,class socket*> socket_map_t;
public:
    static void uninstance();
    static class lnetwork_mgr *instance();

    ~lnetwork_mgr();
    explicit lnetwork_mgr( lua_State *L );

    /* 连接回调 */
    bool connect_new( uint32 conn_id,int32 ecode );
    /* 新增连接 */
    bool accept_new( uint32 conn_id,class socket *new_sk );
    /* 新数据包 */
    void command_new( 
        uint32 conn_id,socket::conn_t conn_ty,const buffer &recv );

    /* 设置指令参数 */
    int32 set_cmd();
    /* 设置(客户端)连接所有者 */
    int32 set_owner();
    /* 设置(服务器)连接session */
    int32 set_session();

    /* 加载schema文件 */
    int32 load_schema();

    /* 发送c2s数据包 */
    int32 send_c2s_packet();
    /* 发送s2c数据包 */
    int32 send_s2c_packet();

    int32 close();
    int32 address ();
    int32 listen  ();
    int32 connect ();

    /* 通过连接id查找所有者 */
    owner_t get_owner( uint32 conn_id );
    /* 通过所有者查找连接id */
    uint32 get_conn_id( owner_t owner );

    /* 通过session获取socket连接 */
    class socket *get_connection( int32 session );

    /* 获取connect_id */
    uint32 generate_connect_id();
    /* 获取指令配置 */
    const cmd_cfg_t *get_cmd_cfg( int32 cmd );
    /* 获取当前服务器session */
    int32 session() { return _session; }
private:
    void delete_socket( uint32 conn_id );
    void process_command( uint32 conn_id,const c2s_header *header );
    /* 转客户端数据包 */
    void clt_forwarding( 
        uint32 conn_id,const c2s_header *header,int32 session );
    /* 客户端数据包回调脚本 */
    void clt_command( uint32 conn_id,
        const char *schema,const char *object,const c2s_header *header );
private:
    lua_State *L;

    int32 _session; /* 当前进程的session */
    uint32 _conn_seed; /* connect_id种子 */

    cmd_map_t _cmd_map;
    socket_map_t _socket_map;

    std::vector<uint32> _deleting;/* 异步删除的socket */
    /* owner-conn_id 映射,ssc数据包转发时需要 */
    map_t<owner_t,uint32> _owner_map;
    /* conn_id-owner 映射，css数据包转发时需要 */
    map_t<uint32,owner_t> _conn_map;
    map_t<int32,uint32> _session_map; /* session-conn_id 映射 */
    static class lnetwork_mgr *_network_mgr;
};

#endif /* __LNETWORK_MGR_H__ */