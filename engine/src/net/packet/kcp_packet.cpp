#include "kcp_packet.hpp"

#if defined(ENABLE_KCP)

#include "net/socket.hpp"

KcpPacket::KcpPacket(class Socket *sk) : Packet(sk)
{
    to_remove_ = 0;
}

KcpPacket::~KcpPacket()
{
}

int32_t KcpPacket::unpack(lua_State *L, Buffer &buffer)
{
    // 与UdpPacket同构，只是帧头从 [u32 size][UdpAddr 24B] 变成 [u32 size]
    // 为了避免拷贝，这里直接返回buffer中的指针并标记下次需要删除的数据
    if (to_remove_)
    {
        buffer.remove_head_data(to_remove_);
        to_remove_ = 0;
    }

    uint32_t size = 0;
    if (!buffer.peek(&size)) return unpack_return(L, PC_MORE);
    if (!buffer.peek_size(size)) return unpack_return(L, PC_MORE);

    // 数据是自己写进去的，正常情况下不会出现异常长度
    if (unlikely(size < sizeof(uint32_t)))
    {
        assert(false);
        return unpack_return(L, PC_MORE);
    }

    buffer.remove_head_data(sizeof(size));
    int32_t len = (int32_t)(size - sizeof(size));

    lua_pushinteger(L, PC_DATA);

    // 空包也返回ud和size，保证lua侧的回调参数固定为(ud, size)
    char *ud = nullptr;
    if (len > 0)
    {
        ud = buffer.peek_buffer(len, 1);
        if (!ud) return unpack_return(L, PC_MORE);
    }
    lua_pushlightuserdata(L, ud);
    lua_pushinteger(L, len);

    to_remove_ = (uint32_t)len;

    return 3; // code, ud, size   ★ 比 UdpPacket 少 addr
}

int32_t KcpPacket::pack(lua_State *L, int32_t index)
{
    // index    : data，lua string 或者 lightuserdata
    // index + 1: data_len，data 为 lightuserdata 时必填
    // index + 2/3: header / header_len，kcp 目前没有业务头，传 nil 即可
    net::SendBuf b;
    net::read_send_args(L, index, b);

    const uint32_t size = (uint32_t)(sizeof(uint32_t) + b.hlen + b.dlen);

    // 小包组合成完整的一帧后一次append，避免中间态
    if (b.hlen + b.dlen <= INLINE_PAYLOAD)
    {
        char frame[sizeof(uint32_t) + INLINE_PAYLOAD];

        memcpy(frame, &size, sizeof(size));
        if (b.hlen > 0) memcpy(frame + sizeof(size), b.hdr, (size_t)b.hlen);
        if (b.dlen > 0)
        {
            memcpy(frame + sizeof(size) + b.hlen, b.data, (size_t)b.dlen);
        }

        socket_->send(frame, (size_t)size); // append + flush

        return 0;
    }

    // 大包分两次append也不会发出半帧：size写全后值才正确，
    // 长度不足时 KcpIO::send 会等下次（收到EV_WRITE再继续）
    char head[sizeof(uint32_t)];
    memcpy(head, &size, sizeof(size));

    socket_->append(head, sizeof(head));
    if (b.hlen > 0) socket_->append(b.hdr, (size_t)b.hlen);
    if (b.dlen > 0) socket_->append(b.data, (size_t)b.dlen);
    socket_->flush();

    return 0;
}

#endif
