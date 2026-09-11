#pragma once

#include "packet.hpp"

#if defined(ENABLE_KCP)

/*
 * kcp的数据打包
 *
 * 缓冲区帧格式：[u32 size][payload]，size含自身(4 + payload)
 *
 * unpack返回：PC_DATA, ud(lightuserdata), size(payload长度)
 * ★ 比 UdpPacket 少一个 addr：对端地址只活在 KcpIO::peer_ 上，
 *   身份由 socket_id（也就是lua侧的self）承载
 */
class KcpPacket : public Packet
{
public:
    virtual ~KcpPacket();
    KcpPacket(class Socket *sk);

    PacketType type() const override { return PT_KCPSTREAM; }

    /// 打包数据，参数为 payload 或 payload + size(lightuserdata时必填)
    virtual int32_t pack_clt(lua_State *L, int32_t index) override;
    virtual int32_t pack_srv(lua_State *L, int32_t index) override;

    int32_t unpack(lua_State *L, Buffer &buffer) override;

private:
    /// payload小于这个值时，直接在栈上组合成完整的一帧，只append一次
    static constexpr int32_t INLINE_PAYLOAD = 2048;

private:
    uint32_t to_remove_; // 已解析完，待删除的buffer
};

#endif
