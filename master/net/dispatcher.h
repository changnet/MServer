#ifndef __DISPATCHER_H__
#define __DISPATCHER_H__

/* 网络数据包分发
 * 1. 分发到其他进程
 * 2. 分发到脚本不同接口
 */

class dispatcher
{
public:
    static void uninstance();
    static class dispatcher *instance();

    /* 连接断开 */
    bool connect_del( uint32 conn_id,int32 conn_ty );
    /* 连接回调 */
    bool connect_new( uint32 conn_id,int32 conn_ty,int32 ecode );
    /* 新增连接 */
    bool accept_new( int32 conn_ty );
    /* 新数据包 */
    void http_command_new( const class socket *sk );
    void command_new( 
        uint32 conn_id,socket::conn_t conn_ty,const buffer &recv );
private:
    dispatcher();
    ~dispatcher();

    void process_command( uint32 conn_id,const c2s_header *header );
    void process_command( uint32 conn_id,const s2s_header *header );
    void process_command( uint32 conn_id,const s2c_header *header );

    void process_css_cmd( uint32 conn_id,const s2s_header *header );
    void process_ssc_cmd( uint32 conn_id,const s2s_header *header );
    void process_rpc_cmd( uint32 conn_id,const s2s_header *header );
    void process_rpc_return( uint32 conn_id,const s2s_header *header );

    /* 转客户端数据包 */
    void clt_forwarding( 
        uint32 conn_id,const c2s_header *header,int32 session );
    /* 数据包回调脚本 */
    void cs_command( uint32 conn_id,owner_t owner,
        const cmd_cfg_t *cfg,const c2s_header *header,bool forwarding = false );
    void sc_command( uint32 conn_id,
        const cmd_cfg_t *cfg,const s2c_header *header );
    void ss_command( uint32 conn_id,
        const cmd_cfg_t *cfg,const s2s_header *header );
private:
    lua_State *L;
    static class dispatcher *_dispatcher;
};

#endif /* __DISPATCHER_H__ */
