#ifndef __WS_STREAM_PACKET_H__
#define __WS_STREAM_PACKET_H__

#include "websocket_packet.h"

class ws_stream_packet : public websocket_packet
{
public:
    ~ws_stream_packet();
    ws_stream_packet( class socket *sk );

    packet_t type() const { return PKT_WSSTREAM; };

    /* 打包服务器发往客户端数据包
    * return: <0 error;>=0 success
    */
    virtual int32 pack_clt( lua_State *L,int32 index );
    /* 打包客户端发往服务器数据包
     * return: <0 error;>=0 success
     */
    virtual int32 pack_srv( lua_State *L,int32 index );

    /* 数据帧完成 */
    virtual int32 on_frame_end();

    int32 raw_pack_clt( 
        int32 cmd,uint16 ecode,const char *ctx,size_t size );
private:
    int32 sc_command();
    int32 cs_command( int32 cmd,const char *ctx,size_t size );
    int32 do_pack_clt( int32 raw_flags,
        int32 cmd,uint16 ecode,const char *ctx,size_t size );
};

#endif /* __WS_STREAM_PACKET_H__ */
