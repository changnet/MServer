#pragma once

#include "http_packet.hpp"

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
    virtual int32_t pack_clt(lua_State *L, int32_t index) override;
    /* 打包客户端发往服务器数据包
     * return: <0 error;>=0 success
     */
    virtual int32_t pack_srv(lua_State *L, int32_t index) override;

    /* 数据解包
     * return: <0 error;0 success
     */
    virtual int32_t unpack(Buffer &buffer);

    /* 控制帧完成 */
    int32_t on_ctrl_end();
    /* 数据帧完成 */
    virtual int32_t on_frame_end();

    // 单个消息时，重置
    class Buffer &body_buffer() { return body_; }

    //< 从http升级到websocket时，会触发一次on_message_complete
    virtual int32_t on_message_complete(bool upgrade) override;

    //< 发送opcode等控制码
    int32_t pack_ctrl(lua_State *L, int32_t index) override;

    /// 设置错误码
    void set_error(int32_t e) { e_ = e; }

protected:
    int32_t invoke_handshake();
    void new_masking_key(char mask[4]);
    int32_t pack_raw(lua_State *L, int32_t index);

protected:
    int32_t e_; /// 错误码 websocket_parser没有提供错误机制，这里自己实现
    bool is_upgrade_;
    class Buffer body_;
    struct websocket_parser *parser_;
};
