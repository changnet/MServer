#ifndef __STREAM_PACKET_H__
#define __STREAM_PACKET_H__

#include "packet.h"
#include "../net_header.h"

struct base_header;
struct c2s_header;
struct s2s_header;
struct cmd_cfg_t;

class stream_packet : public packet
{
public:
    virtual ~stream_packet();
    stream_packet( class socket *sk );

    packet_t type() const { return PKT_STREAM; }

    /* 打包服务器发放客户端数据包
     * return: <0 error;0 incomplete;>0 success
     */
    int32 pack_clt( lua_State *L,int32 index );
    /* 打包客户端发放服务器数据包
     * return: <0 error;0 incomplete;>0 success
     */
    int32 pack_srv( lua_State *L,int32 index );
    int32 pack_ss ( lua_State *L,int32 index );
    int32 pack_rpc( lua_State *L,int32 index );
    int32 pack_ssc( lua_State *L,int32 index );
    int32 pack_ssc_multicast( lua_State *L,int32 index );
    int32 raw_pack_clt(
        int32 cmd,uint16 ecode,const char *ctx,size_t size );
    int32 raw_pack_ss(
        int32 cmd,uint16 ecode,int32 session,const char *ctx,size_t size );
    int32 unpack();
private:
    void dispatch( const struct base_header *header );
    void sc_command( const struct s2c_header *header );
    void cs_dispatch( const struct c2s_header *header );
    void cs_command( int32 cmd,const char *ctx,size_t size );
    void process_ss_command( const s2s_header *header );
    void ss_dispatch( const s2s_header *header );
    void ss_command( const s2s_header *header,const cmd_cfg_t *cmd_cfg );
    void css_command( const s2s_header *header );
    void ssc_command( const s2s_header *header );
    void rpc_command( const s2s_header *header );
    void rpc_return( const s2s_header *header );
    void ssc_multicast( const s2s_header *header );
    int32 do_pack_rpc(
        lua_State *L,int32 unique_id,int32 ecode,uint16 pkt,int32 index );
    void ssc_one_multicast(
        owner_t owner,int32 cmd,uint16 ecode,const char *ctx,int32 size );
};

#endif /* __STREAM_PACKET_H__ */
