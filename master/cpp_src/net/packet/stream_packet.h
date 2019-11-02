#pragma once

#include "packet.h"
#include "../net_header.h"

struct base_header;
struct c2s_header;
struct s2s_header;
struct CmdCfg;

class StreamPacket : public Packet
{
public:
    virtual ~StreamPacket();
    StreamPacket( class Socket *sk );

    PacketType type() const { return PT_STREAM; }

    /* 打包服务器发放客户端数据包
     * return: <0 error;0 incomplete;>0 success
     */
    int32_t pack_clt( lua_State *L,int32_t index );
    /* 打包客户端发放服务器数据包
     * return: <0 error;0 incomplete;>0 success
     */
    int32_t pack_srv( lua_State *L,int32_t index );
    int32_t pack_ss ( lua_State *L,int32_t index );
    int32_t pack_rpc( lua_State *L,int32_t index );
    int32_t pack_ssc( lua_State *L,int32_t index );
    int32_t pack_ssc_multicast( lua_State *L,int32_t index );
    int32_t raw_pack_clt(
        int32_t cmd,uint16_t ecode,const char *ctx,size_t size );
    int32_t raw_pack_ss(
        int32_t cmd,uint16_t ecode,int32_t session,const char *ctx,size_t size );
    int32_t unpack();
private:
    void dispatch( const struct base_header *header );
    void sc_command( const struct s2c_header *header );
    void cs_dispatch( const struct c2s_header *header );
    void cs_command( int32_t cmd,const char *ctx,size_t size );
    void process_ss_command( const s2s_header *header );
    void ss_dispatch( const s2s_header *header );
    void ss_command( const s2s_header *header,const CmdCfg *cmd_cfg );
    void css_command( const s2s_header *header );
    void ssc_command( const s2s_header *header );
    void rpc_command( const s2s_header *header );
    void rpc_return( const s2s_header *header );
    void ssc_multicast( const s2s_header *header );
    int32_t do_pack_rpc(
        lua_State *L,int32_t unique_id,int32_t ecode,uint16_t pkt,int32_t index );
    void ssc_one_multicast(
        Owner owner,int32_t cmd,uint16_t ecode,const char *ctx,int32_t size );
};
