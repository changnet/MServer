#include "ss_stream_packet.hpp"

#include "net/socket.hpp"
#include "lpp/ltools.hpp"

#pragma pack(push, 1)
struct SssHeader
{
    uint32_t size_; // 协议长度，包括自己

    // 后面的字段，应该与ThreadMessage中的定义一致
    int32_t src_;   // 来源地址
    int32_t dst_;   // 目标地址
    int32_t type_;  // 消息类型
    int32_t usize_; // 自定义数据长度
};
#pragma pack(pop)

SsStreamPacket::SsStreamPacket(class Socket *sk) : Packet(sk)
{
    to_remove_ = 0;
}

SsStreamPacket::~SsStreamPacket()
{
}

int32_t SsStreamPacket::unpack(lua_State *L, Buffer &buffer)
{
    // TODO 为了避免拷贝，这里会直接返回buffer中的指针并标记下次需要删除的数据
    // 另一种方式是这里直接xpcall回调到lua，从lua返回后直接删除
    // 但直接回调到Lua不符合外部循环unpack的流程控制，不知道什么时候该中止
    if (to_remove_)
    {
        buffer.remove(to_remove_);
        to_remove_ = 0;
    }

    SssHeader header;
    if (!buffer.peek(&header)) return unpack_return(L, PC_MORE);

    uint32_t size = header.size_;
    if (!buffer.peek_size(size)) return unpack_return(L, PC_MORE);

    to_remove_ = size;
    char *udata = nullptr;
    if (size > sizeof(struct SssHeader))
    {
        udata = buffer.peek_buffer(size, 1);
        udata += sizeof(struct SssHeader);
    }

    lua_pushinteger(L, PC_DATA);
    lua_pushinteger(L, header.src_);
    lua_pushinteger(L, header.dst_);
    lua_pushinteger(L, header.type_);
    lua_pushinteger(L, header.usize_);
    if (udata)
    {
        // 无论是rpc还是转发，最终都要回到C++处理，push一个string到Lua没有意义
        // 这里用lightuserdata减少copy
        lua_pushlightuserdata(L, (void *)udata);
        return 6;
    }

    return 5;
}

int32_t SsStreamPacket::pack_clt(lua_State *L, int32_t index)
{
    return pack_srv(L, index);
}

int32_t SsStreamPacket::pack_srv(lua_State* L, int32_t index)
{
    SssHeader header;
    header.size_  = sizeof(SssHeader);
    header.src_   = luaL_checkinteger32(L, index);
    header.dst_   = luaL_checkinteger32(L, index + 1);
    header.type_  = luaL_checkinteger32(L, index + 2);
    header.usize_ = luaL_checkinteger32(L, index + 3);

    // 如果buffer存在，则usize表示buffer的长度。否则usize是自定义数据
    // 这个和ThreadMessage的机制一致
    if (!lua_islightuserdata(L, index + 4))
    {
        socket_->send(&header, sizeof(SssHeader));
        return 0;
    }

    const char *buffer = (const char *)lua_touserdata(L, index + 4);

    header.size_ += header.usize_;
    socket_->append(&header, sizeof(SssHeader));
    socket_->append(buffer, header.usize_);
    socket_->flush();

    return 0;
}
