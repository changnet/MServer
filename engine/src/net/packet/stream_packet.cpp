#include "stream_packet.hpp"

#include "net/socket.hpp"

StreamPacket::StreamPacket(class Socket *sk) : Packet(sk), to_remove_(0)
{
}

StreamPacket::~StreamPacket()
{
}

int32_t StreamPacket::send(lua_State *L, int32_t index)
{
    net::SendBuf b;
    net::read_send_args(L, index, b);

    const uint32_t total = (uint32_t)(sizeof(uint32_t) + b.hlen + b.dlen);

    // 小包拼成一整帧只append一次：少两次atomic，也不会留下半帧中间态
    if (b.hlen + b.dlen <= INLINE_PAYLOAD)
    {
        char frame[sizeof(uint32_t) + INLINE_PAYLOAD];

        memcpy(frame, &total, sizeof(total));
        if (b.hlen > 0) memcpy(frame + sizeof(total), b.hdr, (size_t)b.hlen);
        if (b.dlen > 0)
        {
            memcpy(frame + sizeof(total) + b.hlen, b.data, (size_t)b.dlen);
        }

        socket_->send(frame, (size_t)total); // append + flush

        return 0;
    }

    // 大包分次append也不会发出半帧：size已写全，读端会等整帧
    char head[sizeof(uint32_t)];
    memcpy(head, &total, sizeof(total));

    socket_->append(head, sizeof(head));
    if (b.hlen > 0) socket_->append(b.hdr, (size_t)b.hlen);
    if (b.dlen > 0) socket_->append(b.data, (size_t)b.dlen);
    socket_->flush();

    return 0;
}

int32_t StreamPacket::unpack(lua_State *L, Buffer &buffer)
{
    // 为了避免拷贝，这里直接返回buffer中的指针并标记下次需要删除的数据
    // 与KcpPacket同构
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
    const int32_t len = (int32_t)(size - sizeof(size));

    // 空包也返回ud和size，保证lua侧的回调参数固定为(ud, size)
    char *ud = nullptr;
    if (len > 0)
    {
        ud = buffer.peek_buffer(len, 1);
        if (!ud) return unpack_return(L, PC_MORE);
    }

    lua_pushinteger(L, PC_DATA);
    lua_pushlightuserdata(L, ud);
    lua_pushinteger(L, len);

    to_remove_ = (uint32_t)len;

    return 3; // code, ud, size
}
