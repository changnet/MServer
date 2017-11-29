#ifndef __WEBSOCKET_PACKET_H__
#define __WEBSOCKET_PACKET_H__

#include "packet.h"

struct websocket_parser;
class websocket_packet : public packet
{
public:
    ~websocket_packet();
    websocket_packet( class socket *sk );

    /* 获取当前packet类型
     */
    packet_t type() const { return PKT_WEBSOCKET; };

    /* 打包服务器发往客户端数据包
     * return: <0 error;>=0 success
     */
    int32 pack_clt( lua_State *L,int32 index );
    /* 打包客户端发往服务器数据包
     * return: <0 error;>=0 success
     */
    int32 pack_srv( lua_State *L,int32 index );

    /* 数据解包 
     * return: <0 error;0 success
     */
    int32 unpack();
private:
    struct websocket_parser *_parser;
};

#endif /* __WEBSOCKET_PACKET_H__ */
