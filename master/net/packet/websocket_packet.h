#ifndef __WEBSOCKET_PACKET_H__
#define __WEBSOCKET_PACKET_H__

#include "../buffer.h"
#include "http_packet.h"

struct websocket_parser;
class websocket_packet : public http_packet
{
public:
    virtual ~websocket_packet();
    websocket_packet( class socket *sk );

    /* 获取当前packet类型
     */
    virtual packet_t type() const { return PKT_WEBSOCKET; };

    /* 打包服务器发往客户端数据包
     * return: <0 error;>=0 success
     */
    virtual int32 pack_clt( lua_State *L,int32 index );
    /* 打包客户端发往服务器数据包
     * return: <0 error;>=0 success
     */
    virtual int32 pack_srv( lua_State *L,int32 index );

    /* 数据解包 
     * return: <0 error;0 success
     */
    virtual int32 unpack();

    /* 控制帧完成 */
    int32 on_ctrl_end();
    /* 数据帧完成 */
    virtual int32 on_frame_end();

    // 单个消息时，重置
    class buffer &body_buffer() { return _body; }

    // 从http升级到websocket时，会触发一次on_message_complete
    int32 on_message_complete( bool upgrade );

    // 发送opcode
    int32 pack_ctrl( lua_State *L,int32 index );
protected:
    int32 invoke_handshake();
    void new_masking_key( char mask[4] );
    int32 pack_raw( lua_State *L,int32 index );
protected:
    bool _is_upgrade;
    class buffer _body;
    struct websocket_parser *_parser;
};

#endif /* __WEBSOCKET_PACKET_H__ */
