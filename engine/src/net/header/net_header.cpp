#include "net/header/net_header.hpp"

/*
 * 端序无关的写/读字节
 *
 * 不用 memcpy 结构体：SssHeader 那种 #pragma pack(1) 直接 memcpy 的写法只在
 * 小端机器上正确，而且没人会注意到它是错的。这里逐个字节移位，两端一致。
 */
static inline char *put_value(char *p, uint64_t v, int32_t width, bool be)
{
    if (be)
    {
        for (int32_t i = width - 1; i >= 0; i--) p[width - 1 - i] = (char)(uint8_t)(v >> (8 * i));
    }
    else
    {
        for (int32_t i = 0; i < width; i++) p[i] = (char)(uint8_t)(v >> (8 * i));
    }

    return p + width;
}

static inline uint64_t get_value(const char *p, int32_t width, bool be)
{
    uint64_t v = 0;

    if (be)
    {
        for (int32_t i = 0; i < width; i++) v = (v << 8) | (uint8_t)p[i];
    }
    else
    {
        for (int32_t i = width - 1; i >= 0; i--) v = (v << 8) | (uint8_t)p[i];
    }

    return v;
}

#ifndef NDEBUG
/*
 * 范围校验（只在 debug 构建）：超范围写进去是静默截断，比如 70000 塞进
 * H2 变成 4464，两端都一致所以永远不报错 —— 这种错最难查。
 * 与 string.pack 一致：只在 width < 8 时检查，8 字节允许任意 64 位位型
 * （I8 传 -1 合法，按补码写）。
 */
static inline void check_range(lua_State *L, lua_Integer v, int32_t width,
                               bool is_signed, int32_t idx)
{
    if (width >= 8) return;

    const int32_t bits = width * 8;

    if (is_signed)
    {
        const lua_Integer lim = (lua_Integer)((uint64_t)1 << (bits - 1));
        if (v < -lim || v >= lim)
        {
            luaL_error(L, "net header arg %d overflow: " LUA_INTEGER_FMT
                          " not fit in signed %d byte",
                       idx, (lua_Integer)v, width);
        }
    }
    else
    {
        const lua_Integer lim = (lua_Integer)((uint64_t)1 << bits);
        if (v < 0 || v >= lim)
        {
            luaL_error(L, "net header arg %d overflow: " LUA_INTEGER_FMT
                          " not fit in unsigned %d byte",
                       idx, (lua_Integer)v, width);
        }
    }
}
#endif

NetHeader::NetHeader() : desc_count_(0)
{
    memset(descs_, 0, sizeof(descs_));
    memset(scratch_, 0, sizeof(scratch_));
}

int32_t NetHeader::define(lua_State *L)
{
    size_t len      = 0;
    const char *fmt = luaL_checklstring(L, 2, &len);

    if (desc_count_ >= MAX_DESC)
    {
        return luaL_error(L, "too many net header defines, max=%d", MAX_DESC);
    }

    Desc &desc      = descs_[desc_count_];
    desc.count      = 0;
    desc.total      = 0;
    desc.big_endian = false;

    const char *p   = fmt;
    const char *end = fmt + len;

    if (p < end && ('<' == *p || '>' == *p))
    {
        desc.big_endian = ('>' == *p);
        ++p;
    }

    int32_t offset = 0;
    while (p < end)
    {
        const char c = *p++;

        // 允许空格分隔，纯粹为了可读性
        if (' ' == c || '\t' == c) continue;

        // 数字后缀，规则同 string.pack：i/I 的宽度由它决定，
        // 其它 token 必须与固有宽度一致
        int32_t suffix     = 0;
        bool has_suffix    = false;
        while (p < end && *p >= '0' && *p <= '9')
        {
            has_suffix = true;
            // 钳一下，反正超过 8 就是非法宽度，避免长数字溢出
            suffix = (suffix > 99) ? 100 : suffix * 10 + (*p - '0');
            ++p;
        }

        int32_t width = 0;
        bool sign     = false;
        switch (c)
        {
        case 'b': width = 1; sign = true; break;
        case 'B': width = 1; break;
        case 'h': width = 2; sign = true; break;
        case 'H': width = 2; break;
        case 'i': width = 4; sign = true; break;
        case 'I': width = 4; break;
        case 'q': width = 8; sign = true; break;
        case 'Q': width = 8; break;
        default:
            return luaL_error(L, "unknow net header token '%c' in \"%s\"", c,
                              fmt);
        }

        if ('i' == c || 'I' == c)
        {
            // 同 string.pack：i 的宽度由后缀决定，省略时为 sizeof(int) == 4
            if (has_suffix) width = suffix;

            if (width < 1 || width > 8)
            {
                return luaL_error(L, "net header \"%s\": %c%d width must be 1~8",
                                  fmt, c, suffix);
            }
        }
        else if (has_suffix && suffix != width)
        {
            // 写了 H4 却只占 2 字节，就是下一个"静默错位"的来源，必须报错
            return luaL_error(L, "net header \"%s\": '%c' is %d byte, suffix "
                              "says %d",
                              fmt, c, width, suffix);
        }

        if (desc.count >= MAX_FIELD)
        {
            return luaL_error(L, "too many fields in net header \"%s\", max=%d",
                              fmt, MAX_FIELD);
        }
        if (offset + width > MAX_HEAD)
        {
            return luaL_error(L, "net header \"%s\" too long, max=%d bytes",
                              fmt, MAX_HEAD);
        }

        Field &field    = desc.fields[desc.count];
        field.offset    = (uint8_t)offset;
        field.width     = (uint8_t)width;
        field.is_signed = sign;

        ++desc.count;
        offset += width;
    }

    desc.total = (uint8_t)offset;

    lua_pushinteger(L, (lua_Integer)desc_count_);
    ++desc_count_; // desc 是 descs_[desc_count_] 的引用，算完再自增

    return 1;
}

const NetHeader::Desc *NetHeader::get_desc(lua_State *L, int32_t &id) const
{
    id = (int32_t)luaL_checkinteger(L, 2);
    if (id < 0 || id >= desc_count_)
    {
        luaL_error(L, "invalid net header id %d", id);
        return nullptr;
    }

    return &descs_[id];
}

int32_t NetHeader::size(lua_State *L) const
{
    int32_t id       = 0;
    const Desc *desc = get_desc(L, id);

    lua_pushinteger(L, (lua_Integer)desc->total);
    return 1;
}

int32_t NetHeader::encode(lua_State *L)
{
    int32_t id       = 0;
    const Desc *desc = get_desc(L, id);

    // 参数：1 self, 2 id, 3.. 各个字段值
    const int32_t argc = lua_gettop(L) - 2;
    if (argc != (int32_t)desc->count)
    {
        // 少传一个会让后面的字段全部错位，这种错最难查，必须报错
        return luaL_error(L, "net header %d needs %d values, got %d", id,
                          (int32_t)desc->count, argc);
    }

    char *p = scratch_;
    for (int32_t i = 0; i < (int32_t)desc->count; i++)
    {
        const Field &field = desc->fields[i];
        const lua_Integer v = luaL_checkinteger(L, 3 + i);

#ifndef NDEBUG
        check_range(L, v, field.width, field.is_signed, 3 + i);
#endif

        p = put_value(p, (uint64_t)v, field.width, desc->big_endian);
    }

    // 指针在实例的 scratch_ 上，下一次 encode 前有效（与 pbc 的契约一致）
    lua_pushlightuserdata(L, (void *)scratch_);
    lua_pushinteger(L, (lua_Integer)desc->total);

    return 2;
}

int32_t NetHeader::decode(lua_State *L)
{
    int32_t id       = 0;
    const Desc *desc = get_desc(L, id);

    if (LUA_TLIGHTUSERDATA != lua_type(L, 3))
    {
        return luaL_error(L, "net header decode expects lightuserdata at arg 3,"
                          " got %s",
                          luaL_typename(L, 3));
    }

    const char *p = (const char *)lua_touserdata(L, 3);
    if (!p)
    {
        // decode 至少要读满一个头，NULL 一定是调用方传错了
        return luaL_error(L, "net header decode got nullptr at arg 3");
    }

    const lua_Integer n = luaL_checkinteger(L, 4);
    if (n < 0 || n > 0x7FFFFFFF)
    {
        return luaL_error(L, "invalid data length at arg 4: " LUA_INTEGER_FMT, n);
    }

    const int32_t len = (int32_t)n;
    if (len < (int32_t)desc->total)
    {
        // 帧损坏，不猜，也不吞错：返回nil让调用方关连接
        lua_pushnil(L);
        return 1;
    }

    for (int32_t i = 0; i < (int32_t)desc->count; i++)
    {
        const Field &field = desc->fields[i];
        uint64_t v = get_value(p + field.offset, field.width, desc->big_endian);

        if (field.is_signed && field.width < 8)
        {
            const int32_t bits = field.width * 8;
            if (v & (1ULL << (bits - 1))) v |= ~((1ULL << bits) - 1);
        }

        lua_pushinteger(L, (lua_Integer)v);
    }

    const int32_t body_len = len - (int32_t)desc->total;
    if (body_len > 0)
    {
        lua_pushlightuserdata(L, (void *)(p + desc->total));
    }
    else
    {
        /* 空载荷给nil，不给"指向帧尾的指针"：
         * ss的心跳包里没有payload时，头里的usize是自定义数据而不是长度，
         * 如果回一个合法的指针，调用方很容易拿usize去读，直接越界 */
        lua_pushnil(L);
    }
    lua_pushinteger(L, (lua_Integer)body_len);

    return (int32_t)desc->count + 2;
}

int32_t NetHeader::format(lua_State *L) const
{
    int32_t id       = 0;
    const Desc *desc = get_desc(L, id);

    char buff[MAX_FIELD * 2 + 2];
    int32_t n = 0;

    buff[n++] = desc->big_endian ? '>' : '<';
    for (int32_t i = 0; i < (int32_t)desc->count; i++)
    {
        const Field &field = desc->fields[i];

        char c = '?';
        switch (field.width)
        {
        case 1: c = 'b'; break;
        case 2: c = 'h'; break;
        case 4: c = 'i'; break;
        case 8: c = 'q'; break;
        default: break;
        }

        if (!field.is_signed) c = (char)(c - 'a' + 'A');
        buff[n++] = c;
    }
    buff[n] = '\0';

    lua_pushlstring(L, buff, (size_t)n);
    return 1;
}
