#pragma once

#include <map>
#include <queue>

#include "packet.hpp"

struct http_parser;
class HttpPacket : public Packet
{
public:
    typedef std::map<std::string, std::string> head_map_t;
    struct http_info
    {
        std::string _url;
        std::string _body;
        head_map_t _head_field;
    };

public:
    virtual ~HttpPacket();
    explicit HttpPacket(class Socket *sk);

    virtual PacketType type() const { return PT_HTTP; }

    virtual int32_t pack_clt(lua_State *L, int32_t index);
    virtual int32_t pack_srv(lua_State *L, int32_t index);
    /* 数据解包
     * return: <0 error;0 success
     */
    virtual int32_t unpack();
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
    struct http_info _http_info;

private:
    int32_t pack_raw(lua_State *L, int32_t index);

    http_parser *_parser;
    std::string _cur_field;
    std::string _cur_value;
};
