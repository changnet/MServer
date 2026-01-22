#pragma once

#include <map>
#include <queue>

#include <llhttp.h>

#include "packet.hpp"

/**
 * @brief Http编码解码
 * TODO 使用std::string来解析协议容易造成内存浪费，增加内存碎片
 * 应该做一个内存池，但游戏服务器http通常只是铺助，用得不多就不搞了
 */
class HttpPacket : public Packet
{
public:
    using HeadField = std::map<std::string, std::string>;
    struct http_info
    {
        std::string url_;
        std::string body_;
        HeadField head_field_;
    };

public:
    virtual ~HttpPacket();
    explicit HttpPacket(class Socket *sk);

    virtual PacketType type() const override { return PT_HTTP; }

    virtual int32_t pack_clt(lua_State *L, int32_t index) override;
    virtual int32_t pack_srv(lua_State *L, int32_t index) override;

    virtual int32_t unpack(lua_State *L, Buffer &buffer) override;

    virtual int32_t unpack_on_closed(lua_State *L) override;

    /* 解压http数据到lua堆栈 */
    int32_t unpack_header(lua_State *L) const;

public:
    /* http_parse 回调函数 */
    void reset();
    void on_headers_complete();
    int32_t on_message_complete(bool upgrade);
    void append_url(const char *at, size_t len);
    void append_body(const char *at, size_t len);
    void append_cur_field(const char *at, size_t len);
    void append_cur_value(const char *at, size_t len);

protected:
    lua_State *L_; // 解析时回调临时存一下，其他情况不要用这个
    struct http_info http_info_;

private:
    virtual int32_t on_upgrade(lua_State *L);
    int32_t pack_any(lua_State *L, int32_t index);

    llhttp_t parser_;
    std::string cur_field_;
    std::string cur_value_;
};
