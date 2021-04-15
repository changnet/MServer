#include <http_parser.h>

#include "../../lua_cpplib/ltools.hpp"
#include "../../system/static_global.hpp"
#include "../socket.hpp"
#include "http_packet.hpp"

#define ASSERT_PARSER_DATA() \
    assert(parser && (parser->data))

// 开始解析报文，第一个回调的函数，在这里初始化数据
int32_t on_message_begin(http_parser *parser)
{
    ASSERT_PARSER_DATA();

    class HttpPacket *http_packet = static_cast<class HttpPacket *>(parser->data);
    // 这个千万不要因为多态调到websocket_packet::reset去了
    http_packet->HttpPacket::reset();

    return 0;
}

// 解析到url报文，可能只是一部分
int32_t on_url(http_parser *parser, const char *at, size_t length)
{
    ASSERT_PARSER_DATA();

    class HttpPacket *http_packet = static_cast<class HttpPacket *>(parser->data);
    http_packet->append_url(at, length);

    return 0;
}

int32_t on_status(http_parser *parser, const char *at, size_t length)
{
    UNUSED(parser);
    UNUSED(at);
    UNUSED(length);
    // parser->status_code 本身有缓存，这里不再缓存
    return 0;
}

int32_t on_header_field(http_parser *parser, const char *at, size_t length)
{
    ASSERT_PARSER_DATA();

    class HttpPacket *http_packet = static_cast<class HttpPacket *>(parser->data);
    http_packet->append_cur_field(at, length);

    return 0;
}

int32_t on_header_value(http_parser *parser, const char *at, size_t length)
{
    ASSERT_PARSER_DATA();

    class HttpPacket *http_packet = static_cast<class HttpPacket *>(parser->data);
    http_packet->append_cur_value(at, length);

    return 0;
}

int32_t on_headers_complete(http_parser *parser)
{
    ASSERT_PARSER_DATA();

    class HttpPacket *http_packet = static_cast<class HttpPacket *>(parser->data);
    http_packet->on_headers_complete();

    return 0;
}

int32_t on_body(http_parser *parser, const char *at, size_t length)
{
    ASSERT_PARSER_DATA();

    class HttpPacket *http_packet = static_cast<class HttpPacket *>(parser->data);
    http_packet->append_body(at, length);

    return 0;
}

int32_t on_message_complete(http_parser *parser)
{
    ASSERT_PARSER_DATA();

    class HttpPacket *http_packet = static_cast<class HttpPacket *>(parser->data);

    // 这里返回error将不再继续解析
    if (http_packet->on_message_complete(parser->upgrade))
    {
        return HPE_PAUSED;
    }

    return HPE_OK;
}

/**
 * http chunk应该用不到，暂不处理
 * https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Transfer-Encoding
HTTP/1.1 200 OK
Content-Type: text/plain
Transfer-Encoding: chunked

7\r\n
Mozilla\r\n
9\r\n
Developer\r\n
7\r\n
Network\r\n
0\r\n
\r\n
 */
static const struct http_parser_settings settings = {on_message_begin,
                                                     on_url,
                                                     on_status,
                                                     on_header_field,
                                                     on_header_value,
                                                     on_headers_complete,
                                                     on_body,
                                                     on_message_complete,

                                                     NULL,
                                                     NULL};

/* ====================== HTTP FUNCTION END ================================ */
HttpPacket::~HttpPacket()
{
    delete _parser;
    _parser = NULL;
}

HttpPacket::HttpPacket(class Socket *sk) : Packet(sk)
{
    // HTTP_REQUEST, HTTP_RESPONSE, HTTP_BOTH
    _parser = new struct http_parser();
    http_parser_init(_parser, HTTP_BOTH);
    _parser->data = this;
}

int32_t HttpPacket::unpack()
{
    class Buffer &recv = _socket->recv_buffer();
    uint32_t size      = recv.get_used_size();
    if (size == 0) return 0;

    /* 注意：解析完成后，是由http-parser回调脚本的，这时脚本那边可能会关闭socket
     * 因此要注意http_parser_execute后部分资源是不可再访问的
     * http是收到多少解析多少，因此不存在使用多个缓冲区chunk的情况，用get_used_ctx即可，
     * 不用check_all_used_ctx
     */
    int32_t nparsed =
        http_parser_execute(_parser, &settings, recv.get_used_ctx(), size);

    /* web_socket报文,暂时不用回调到上层
     * The user is expected to check if parser->upgrade has been set to 1 after
     * http_parser_execute() returns. Non-HTTP data begins at the buffer
     * supplied offset by the return value of http_parser_execute()
     */
    if (_parser->upgrade)
    {
        /* 除去缓冲区中websocket握手数据
         * 返回 >0 由子类websocket_packet继续处理数据
         */
        recv.remove(nparsed);
        return 1;
    }

    recv.clear();                 // http_parser不需要旧缓冲区
    if (nparsed != (int32_t)size) /* error */
    {
        int32_t no = _parser->http_errno;
        ELOG("http parse error(%d):%s", no,
              http_errno_name(static_cast<enum http_errno>(no)));

        return -1;
    }

    return 0;
}

void HttpPacket::reset()
{
    _cur_field.clear();
    _cur_value.clear();

    _http_info._url.clear();
    _http_info._body.clear();
    _http_info._head_field.clear();
}

void HttpPacket::on_headers_complete()
{
    if (_cur_field.empty()) return;

    _http_info._head_field[_cur_field] = _cur_value;
}

int32_t HttpPacket::on_message_complete(bool upgrade)
{
    UNUSED(upgrade);
    static lua_State *L = StaticGlobal::state();
    assert(0 == lua_gettop(L));

    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, "command_new");
    lua_pushinteger(L, _socket->conn_id());

    // enum http_parser_type 0请求request，1 返回respond
    lua_pushinteger(L, _parser->type);
    lua_pushinteger(L, _parser->status_code); // 仅respond有用

    // GET or POST
    // const char *method_str =
    //     http_method_str(static_cast<enum http_method>(_parser->method));
    // lua_pushstring(L, method_str);
    lua_pushinteger(L, _parser->method); // 1 = GET, 3 = POST，仅request有用
    lua_pushstring(L, _http_info._url.c_str());
    lua_pushstring(L, _http_info._body.c_str());

    if (EXPECT_FALSE(LUA_OK != lua_pcall(L, 6, 0, 1)))
    {
        ELOG("command_new:%s", lua_tostring(L, -1));
    }

    lua_settop(L, 0); /* remove traceback */

    /* 注意：如果一次收到多个http消息，需要在这里检测socket是否由上层脚本关闭
     * 然后终止http-parser的解析
     */
    return _socket->fd() < 0 ? -1 : 0;
}

void HttpPacket::append_url(const char *at, size_t len)
{
    _http_info._url.append(at, len);
}

void HttpPacket::append_body(const char *at, size_t len)
{
    _http_info._body.append(at, len);
}

void HttpPacket::append_cur_field(const char *at, size_t len)
{
    /* 报文中的field和value是成对的，但是http-parser解析完一对字段后并没有回调任何函数
     * 如果检测到value不为空，则说明当前收到的是新字段
     */
    if (!_cur_value.empty())
    {
        _http_info._head_field[_cur_field] = _cur_value;

        _cur_field.clear();
        _cur_value.clear();
    }

    _cur_field.append(at, len);
}

void HttpPacket::append_cur_value(const char *at, size_t len)
{
    _cur_value.append(at, len);
}

int32_t HttpPacket::unpack_header(lua_State *L) const
{
    const head_map_t &head_field = _http_info._head_field;

    // table赋值时，需要一个额外的栈
    if (!lua_checkstack(L, 2))
    {
        ELOG("http unpack header stack over flow");
        return -1;
    }

    lua_newtable(L);
    head_map_t::const_iterator head_itr = head_field.begin();
    for (; head_itr != head_field.end(); head_itr++)
    {
        lua_pushstring(L, head_itr->second.c_str());
        lua_setfield(L, -2, head_itr->first.c_str());
    }

    return 1;
}

/* http的GET、POST都由上层处理好再传入底层
 */
int32_t HttpPacket::pack_raw(lua_State *L, int32_t index)
{
    size_t size     = 0;
    const char *ctx = luaL_checklstring(L, index, &size);
    if (!ctx) return 0;

    _socket->append(ctx, size);

    return 0;
}

int32_t HttpPacket::pack_clt(lua_State *L, int32_t index)
{
    return pack_raw(L, index);
}

int32_t HttpPacket::pack_srv(lua_State *L, int32_t index)
{
    return pack_raw(L, index);
}
