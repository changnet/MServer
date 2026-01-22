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
    virtual PacketType type() const override
    {
        return PT_WEBSOCKET;
    }

    /**
     * 打包服务器发往客户端数据包
     * @return <0 error;>=0 success
     */
    virtual int32_t pack_clt(lua_State *L, int32_t index) override;
    /**
     * 打包客户端发往服务器数据包
     * @return <0 error;>=0 success
     */
    virtual int32_t pack_srv(lua_State *L, int32_t index) override;

    /**
     * 数据解包，把结果放到Lua堆栈
     * @return 堆栈新增的变量数量
     */
    virtual int32_t unpack(lua_State *L, Buffer &buffer) override;

    // 控制帧(ping、pong等opcode)完成
    int32_t on_ctrl_end();
    // 数据帧完成
    virtual int32_t on_frame_end();

    // 如果是第一次解析到数据，则标记数据开始
    void try_set_payload(const char *payload)
    {
        if (!payload_) payload_ = payload;
    }

    // 发送opcode等控制码
    int32_t pack_ctrl(lua_State *L, int32_t index) override;

protected:
    void new_masking_key(char mask[4]);
    // 获取payload的指针及数据大小
    const char *make_payload(size_t &size);
    // 打包websocket header
    void pack_header(Buffer &buffer, websocket_flags flags, const char mask[4],
                     const char *data, size_t data_len);
    void pack_frame(Buffer &buffer, websocket_flags flags, const char mask[4],
                    const char *data, size_t data_len, uint8_t *mask_offset);

private:
    int32_t pack_any(lua_State *L, int32_t index);
    virtual int32_t on_upgrade(lua_State *L) override;

protected:
    int32_t e_; /// 错误码 websocket_parser没有提供错误机制，这里自己实现
    bool is_upgrade_;
    size_t require_len_; // 需要的总长度（不是header+payload的长度）
    size_t to_remove_;   // 上一次待删除的数据
    size_t head_len_;    // head占的大小（不一定是整个head）
    const char *payload_; // 第一次解析到数据的地方
    struct websocket_parser *parser_;
};
