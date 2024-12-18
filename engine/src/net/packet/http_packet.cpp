#include "lpp/ltools.hpp"
#include "system/static_global.hpp"
#include "net/socket.hpp"

#include "http_packet.hpp"

// 开始解析报文，第一个回调的函数，在这里初始化数据
int32_t on_message_begin(llhttp_t *parser)
{
    class HttpPacket *http_packet = static_cast<class HttpPacket *>(parser->data);
    // 这个千万不要因为多态调到websocket_packet::reset去了
    http_packet->HttpPacket::reset();

    return 0;
}

// 解析到url报文，可能只是一部分
int32_t on_url(llhttp_t *parser, const char *at, size_t length)
{
    class HttpPacket *http_packet = static_cast<class HttpPacket *>(parser->data);
    http_packet->append_url(at, length);

    return 0;
}

int32_t on_status(llhttp_t *parser, const char *at, size_t length)
{
    UNUSED(parser);
    UNUSED(at);
    UNUSED(length);
    // parser->status_code 本身有缓存，这里不再缓存
    return 0;
}

int32_t on_header_field(llhttp_t *parser, const char *at, size_t length)
{
    class HttpPacket *http_packet = static_cast<class HttpPacket *>(parser->data);
    http_packet->append_cur_field(at, length);

    return 0;
}

int32_t on_header_value(llhttp_t *parser, const char *at, size_t length)
{
    class HttpPacket *http_packet = static_cast<class HttpPacket *>(parser->data);
    http_packet->append_cur_value(at, length);

    return 0;
}

int32_t on_headers_complete(llhttp_t *parser)
{
    class HttpPacket *http_packet = static_cast<class HttpPacket *>(parser->data);
    http_packet->on_headers_complete();

    return 0;
}

int32_t on_body(llhttp_t *parser, const char *at, size_t length)
{
    class HttpPacket *http_packet = static_cast<class HttpPacket *>(parser->data);
    http_packet->append_body(at, length);

    return 0;
}

int32_t on_message_complete(llhttp_t *parser)
{
    class HttpPacket *http_packet = static_cast<class HttpPacket *>(parser->data);

    // on_message_complete里回调socket关闭中断解析
    // 但不要报错，报错会直接销毁这个packet对象。socket关闭外层有检测，不在这里处理
    if (http_packet->on_message_complete(parser->upgrade))
    {
        return HPE_PAUSED;
    }

    return HPE_OK;
}

/**
 * 临时用于初始化设置
 */
class HttpSettingInitializer
{
public:
    HttpSettingInitializer()
    {
        llhttp_settings_init(&setting_);

        /**
         * http chunk应该用不到，暂不处理(on_chunk_complete)
         * https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Transfer-Encoding
         */

        setting_.on_message_begin    = on_message_begin;
        setting_.on_url              = on_url;
        setting_.on_status           = on_status;
        setting_.on_header_field     = on_header_field;
        setting_.on_header_value     = on_header_value;
        setting_.on_headers_complete = on_headers_complete;
        setting_.on_body             = on_body;
        setting_.on_message_complete = on_message_complete;
    }

public:
    llhttp_settings_t setting_;
};

static const HttpSettingInitializer initializer;

/* ====================== HTTP FUNCTION END ================================ */
HttpPacket::~HttpPacket()
{
}

HttpPacket::HttpPacket(class Socket *sk) : Packet(sk)
{
    // HTTP_REQUEST, HTTP_RESPONSE, HTTP_BOTH
    llhttp_init(&parser_, HTTP_BOTH, &initializer.setting_);
    parser_.data = this;
}

int32_t HttpPacket::unpack(Buffer &buffer)
{
    // http是收到多少解析多少，因此不存在使用多个缓冲区chunk的情况
    size_t size = 0;
    bool next        = false;
    const char *data = buffer.get_front_used(size, next);
    if (size == 0) return 0;

    /* 注意：解析完成后，是由parser回调脚本的，这时脚本那边可能会关闭socket
     * 因此要注意execute后部分资源是不可再访问的
     * 一旦出错，llhttp_execute总是返回之前的错误码，不会继续执行，除非重新init或者resume
     */
    enum llhttp_errno e = llhttp_execute(&parser_, data, size);

    /**
     * 连接升级为websocket，返回1，后续的数据由websocket那边继续处理
     */
    if (HPE_PAUSED_UPGRADE == e)
    {
        bool next = buffer.remove(llhttp_get_error_pos(&parser_) - data);
        return next ? 1 : 0;
    }

    buffer.clear(); // http_parser不需要旧缓冲区

    // PAUSE通常是上层脚本关闭了socket，需要中止解析
    if (HPE_OK != e && HPE_PAUSED != e)
    {
        ELOG("http parse error(%d):%s", llhttp_get_errno(&parser_),
             llhttp_get_error_reason(&parser_));

        return -1;
    }

    return 0;
}

void HttpPacket::reset()
{
    cur_field_.clear();
    cur_value_.clear();

    http_info_.url_.clear();
    http_info_.body_.clear();
    http_info_.head_field_.clear();
}

void HttpPacket::on_headers_complete()
{
    if (cur_field_.empty()) return;

    http_info_.head_field_[cur_field_] = cur_value_;
}

int32_t HttpPacket::on_message_complete(bool upgrade)
{
    UNUSED(upgrade);
    lua_State *L = StaticGlobal::L;
    assert(0 == lua_gettop(L));

    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, "command_new");
    lua_pushinteger(L, socket_->conn_id());

    // HTTP_REQUEST = 1, HTTP_RESPONSE = 2
    lua_pushinteger(L, parser_.type);
    lua_pushinteger(L, parser_.status_code); // 仅respond有用

    // 1 = GET, 3 = POST，仅request有用
    lua_pushinteger(L, parser_.method);
    lua_pushstring(L, http_info_.url_.c_str());

    int32_t args = 5;
    if (http_info_.body_.length() > 0)
    {
        args++;
        lua_pushstring(L, http_info_.body_.c_str());
    }

    if (unlikely(LUA_OK != lua_pcall(L, args, 0, 1)))
    {
        ELOG("command_new:%s", lua_tostring(L, -1));
    }

    lua_settop(L, 0); /* remove traceback */

    /* 注意：如果一次收到多个http消息，需要在这里检测socket是否由上层脚本关闭
     * 然后终止http-parser的解析
     */
    return socket_->fd() < 0 ? -1 : 0;
}

void HttpPacket::append_url(const char *at, size_t len)
{
    http_info_.url_.append(at, len);
}

void HttpPacket::append_body(const char *at, size_t len)
{
    http_info_.body_.append(at, len);
}

void HttpPacket::append_cur_field(const char *at, size_t len)
{
    /* 报文中的field和value是成对的，但是http-parser解析完一对字段后并没有回调任何函数
     * 如果检测到value不为空，则说明当前收到的是新字段
     */
    if (!cur_value_.empty())
    {
        http_info_.head_field_[cur_field_] = cur_value_;

        cur_field_.clear();
        cur_value_.clear();
    }

    cur_field_.append(at, len);
}

void HttpPacket::append_cur_value(const char *at, size_t len)
{
    cur_value_.append(at, len);
}

int32_t HttpPacket::unpack_header(lua_State *L) const
{
    const head_map_t &head_field = http_info_.head_field_;

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

    socket_->send(ctx, size);

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

void HttpPacket::on_closed()
{
    // 没有 Content-Length 时，以连接关闭时读到的数据为准
    // 这时候需要手动调用llhttp_finish来触发on_message_complete
    if (!llhttp_message_needs_eof(&parser_)) return;

    llhttp_errno_t e = llhttp_finish(&parser_);
    if (HPE_OK != e && HPE_PAUSED != e)
    {
        ELOG("llhttp_finish erro (%d): %s", e, llhttp_get_error_reason(&parser_));
    }
}