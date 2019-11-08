#pragma once

#include "../buffer.h"
#include "http_packet.h"

struct websocket_parser;
class WebsocketPacket : public HttpPacket
{
public:
    virtual ~WebsocketPacket();
    WebsocketPacket(class Socket *sk);

    /* 获取当前packet类型
     */
    virtual PacketType type() const { return PT_WEBSOCKET; }

    /* 打包服务器发往客户端数据包
     * return: <0 error;>=0 success
     */
    virtual int32_t pack_clt(lua_State *L, int32_t index);
    /* 打包客户端发往服务器数据包
     * return: <0 error;>=0 success
     */
    virtual int32_t pack_srv(lua_State *L, int32_t index);

    /* 数据解包
     * return: <0 error;0 success
     */
    virtual int32_t unpack();

    /* 控制帧完成 */
    int32_t on_ctrl_end();
    /* 数据帧完成 */
    virtual int32_t on_frame_end();

    // 单个消息时，重置
    class Buffer &body_buffer() { return _body; }

    // 从http升级到websocket时，会触发一次on_message_complete
    int32_t on_message_complete(bool upgrade);

    // 发送opcode
    int32_t pack_ctrl(lua_State *L, int32_t index);

protected:
    int32_t invoke_handshake();
    void new_masking_key(char mask[4]);
    int32_t pack_raw(lua_State *L, int32_t index);

protected:
    bool _is_upgrade;
    class Buffer _body;
    struct websocket_parser *_parser;
};
