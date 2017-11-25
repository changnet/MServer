#ifndef __STREAM_PACKET_H__
#define __STREAM_PACKET_H__

#include "packet.h"

struct lua_State;
struct base_header;
struct c2s_header;
struct s2s_header;
struct cmd_cfg_t;

class stream_packet : public packet
{
public:
    virtual ~stream_packet();
    stream_packet( class socket *sk );

    int32 pack();
    int32 unpack();
private:
    void dispatch( const struct base_header *header );
    void sc_command( const struct s2c_header *header );
    void cs_dispatch( const struct c2s_header *header );
    void clt_forwarding( const c2s_header *header,int32 session );
    void cs_command( const c2s_header *header,const cmd_cfg_t *cmd_cfg );
    void process_ss_command( const s2s_header *header );
    void ss_dispatch( const s2s_header *header );
    void ss_command( const s2s_header *header,const cmd_cfg_t *cmd_cfg );
    void css_command( const s2s_header *header );
    void ssc_command( const s2s_header *header );
    void rpc_command( const s2s_header *header );
    void rpc_return( const s2s_header *header );
    int32 rpc_pack(
        lua_State *L,int32 unique_id,int32 ecode,uint16 pkt,int32 index );
};

#endif /* __STREAM_PACKET_H__ */
