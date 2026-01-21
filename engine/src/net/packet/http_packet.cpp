#include "lpp/ltools.hpp"
#include "system/static_global.hpp"
#include "net/socket.hpp"

#include "http_packet.hpp"

// 开始解析报文，第一个回调的函数，在这里初始化数据
static int32_t on_message_begin(llhttp_t *parser)
{
    class HttpPacket *http_packet = static_cast<class HttpPacket *>(parser->data);
    // 这个千万不要因为多态调到websocket_packet::reset去了
    http_packet->HttpPacket::reset();

    return 0;
}

// 解析到url报文，可能只是一部分
static int32_t on_url(llhttp_t *parser, const char *at, size_t length)
{
    class HttpPacket *http_packet = static_cast<class HttpPacket *>(parser->data);
    http_packet->append_url(at, length);

    return 0;
}

static int32_t on_status(llhttp_t *parser, const char *at, size_t length)
{
    UNUSED(parser);
    UNUSED(at);
    UNUSED(length);
    // parser->status_code 本身有缓存，这里不再缓存
    return 0;
}

static int32_t on_header_field(llhttp_t *parser, const char *at, size_t length)
{
    class HttpPacket *http_packet = static_cast<class HttpPacket *>(parser->data);
    http_packet->append_cur_field(at, length);

    return 0;
}

static int32_t on_header_value(llhttp_t *parser, const char *at, size_t length)
{
    class HttpPacket *http_packet = static_cast<class HttpPacket *>(parser->data);
    http_packet->append_cur_value(at, length);

    return 0;
}

static int32_t on_headers_complete(llhttp_t *parser)
{
    class HttpPacket *http_packet = static_cast<class HttpPacket *>(parser->data);
    http_packet->on_headers_complete();

    return 0;
}

static int32_t on_body(llhttp_t *parser, const char *at, size_t length)
{
    class HttpPacket *http_packet = static_cast<class HttpPacket *>(parser->data);
    http_packet->append_body(at, length);

    return 0;
}

static int32_t on_message_complete(llhttp_t *parser)
{
    class HttpPacket *http_packet = static_cast<class HttpPacket *>(parser->data);

    return http_packet->on_message_complete(parser->upgrade);
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
    L_ = nullptr;
    // HTTP_REQUEST, HTTP_RESPONSE, HTTP_BOTH
    llhttp_init(&parser_, HTTP_BOTH, &initializer.setting_);
    parser_.data = this;
}

int32_t HttpPacket::unpack(lua_State *L, Buffer &buffer)
{
    // http是收到多少解析多少，因此不存在使用多个缓冲区chunk的情况
    size_t size      = 0;
    const char *data = buffer.get_front_data(size);
    if (size == 0) return unpack_return(L, PC_MORE);

    L_              = L;
    int32_t old_top = lua_gettop(L);

    /**
     * 只要e(即on_message_complete等回调)不为HPE_OK，再次调用llhttp_execute总是返回之前的错误码
     * 如果返回的是HPE_PAUSE，则需要保留data缓冲区不变，下次用llhttp_resume重置错误码
     *
     * 如果是其他错误码，则需要调用llhttp_init()重新初始化。但缓冲区中的数据就被丢弃了
     * 因此一般情况下，一旦解析出错，直接断开重连更合理些。
     *
     * 现在解析到的数据，是存在http_info_中，相当于拷贝了一份，效率相当低
     * 不过游戏中http用得不多，暂不处理
     */

    enum llhttp_errno e = llhttp_execute(&parser_, data, size);

    if (HPE_OK == e)
    {
        // 只解析到一部分数据，还不是完整的message
        buffer.remove(size); // 数据已解析完不需要旧缓冲区了

        // 一个message的数据可能包含在多个缓冲区，继续解析
        return unpack(L, buffer);
    }
    else if (HPE_PAUSED == e)
    {
        llhttp_resume(&parser_);
        // 成功解析出数据，数据已经在on_message_complete中设置到lua的堆栈
        buffer.remove(llhttp_get_error_pos(&parser_) - data);
        int32_t new_top = lua_gettop(L);
        return new_top - old_top;
    }

    ELOG("http parse error(%d):%s", llhttp_get_errno(&parser_),
         llhttp_get_error_reason(&parser_));

    return unpack_error(L, "http parse error");
}
int32_t HttpPacket::on_upgrade(lua_State *L)
{
    lua_pushinteger(L, PC_UPGRADE);
    return 1;
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
    // 这里不支持返回HPE_PAUSED_UPGRADE
    if (unlikely(upgrade))
    {
        on_upgrade(L_);
        return HPE_PAUSED;
    }

    lua_checkstack(L_, 6);

    lua_pushinteger(L_, PC_DATA);

    // HTTP_REQUEST = 1, HTTP_RESPONSE = 2
    lua_pushinteger(L_, parser_.type);
    lua_pushinteger(L_, parser_.status_code); // 仅respond有用

    // 1 = GET, 3 = POST，仅request有用
    lua_pushinteger(L_, parser_.method);
    lua_pushstring(L_, http_info_.url_.c_str());

    if (http_info_.body_.length() > 0)
    {
        lua_pushstring(L_, http_info_.body_.c_str());
    }

    return HPE_PAUSED;
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
int32_t HttpPacket::pack_any(lua_State *L, int32_t index)
{
    size_t size     = 0;
    const char *ctx = luaL_checklstring(L, index, &size);
    if (!ctx) return 0;

    socket_->send(ctx, size);

    return 0;
}

int32_t HttpPacket::pack_clt(lua_State *L, int32_t index)
{
    return pack_any(L, index);
}

int32_t HttpPacket::pack_srv(lua_State *L, int32_t index)
{
    return pack_any(L, index);
}

int32_t HttpPacket::unpack_on_closed(lua_State *L)
{
    // 没有 Content-Length 时，以连接关闭时读到的数据为准
    // 这时候需要手动调用llhttp_finish来触发on_message_complete
    if (!llhttp_message_needs_eof(&parser_)) return unpack_return(L, PC_MORE);

    int32_t old_top  = lua_gettop(L);
    L_               = L;
    llhttp_errno_t e = llhttp_finish(&parser_);

    // 成功解析出数据，数据已经在on_message_complete中设置到lua的堆栈
    if (HPE_PAUSED == e)
    {
        int32_t new_top = lua_gettop(L);
        return new_top - old_top;
    }

    ELOG("llhttp_finish error(%d):%s", llhttp_get_errno(&parser_),
         llhttp_get_error_reason(&parser_));

    return unpack_error(L, "http parse error");
}