#pragma once

#include "packet.hpp"

/*
 * 二进制流打包（服务器之间、以及自定义的 tcp 长连接）
 *
 * 缓冲区帧格式：[u32 size][header][payload]，size 含自身(4 + header + payload)
 * header 是 NetHeader 编出来的业务头，Packet 完全不解释它
 *
 * unpack 返回：PC_DATA, ud(lightuserdata), size(payload长度=header+data)
 * 空包也返回 (nullptr, 0)，保证 lua 侧回调参数固定为 (ud, size)
 */
class StreamPacket : public Packet
{
public:
    virtual ~StreamPacket();
    StreamPacket(class Socket *sk);

    PacketType type() const override { return PT_STREAM; }

    /// 打包数据，参数为 data[, data_len], header[, header_len]
    virtual int32_t pack(lua_State *L, int32_t index) override;

    int32_t unpack(lua_State *L, Buffer &buffer) override;

private:
    uint32_t to_remove_; // 已解析完，待删除的buffer
};
