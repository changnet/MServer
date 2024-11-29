#pragma once

#include <map>
#include <queue>

#include <llhttp.h>

#include "packet.hpp"

/**
 * Http编码解码
 */
class HttpPacket : public Packet
{
public:
    // TODO 使用std::string来解析协议效率不高，应该做一个内存池
    // 但游戏服务器http通常只是铺助，要求不高
    typedef std::map<std::string, std::string> head_map_t;
    struct http_info
    {
        std::string url_;
        std::string body_;
        head_map_t head_field_;
    };

public:
    virtual ~HttpPacket();
    explicit HttpPacket(class Socket *sk);

    virtual PacketType type() const override { return PT_HTTP; }

    virtual int32_t pack_clt(lua_State *L, int32_t index) override;
    virtual int32_t pack_srv(lua_State *L, int32_t index) override;

    virtual int32_t unpack(Buffer &buffer) override;

    virtual void on_closed() override;

    /* 解压http数据到lua堆栈 */
    int32_t unpack_header(lua_State *L) const;

public:
    /* http_parse 回调函数 */
    void reset();
    void on_headers_complete();
    virtual int32_t on_message_complete(bool upgrade);
    void append_url(const char *at, size_t len);
    void append_body(const char *at, size_t len);
    void append_cur_field(const char *at, size_t len);
    void append_cur_value(const char *at, size_t len);

protected:
    struct http_info http_info_;

private:
    int32_t pack_raw(lua_State *L, int32_t index);

    llhttp_t parser_;
    std::string cur_field_;
    std::string cur_value_;
};
