#include <lua.hpp>
#include <stdexcept>

#include "lua_codec.hpp"

// 缓冲区的大小
static const int32_t MIN_BUFF = 64 * 1024;
static const int32_t MAX_BUFF = 8 * 1024 * 1024;

// 编码的变量最大数量
static const int32_t MAX_VARIABLE = 255;

// 支持编码的数据类型，不能直接用lua本身的类型T_NIL之类的，因为没有区分整形
enum LuaType
{
    LT_NIL   = 0, // nil值
    LT_BOOL  = 1, // boolean类型
    LT_NUM   = 2, // 浮点型
    LT_STR   = 3, // 字符串
    LT_TABLE = 4, // table
    LT_INT   = 5, // 整形
};

LuaCodec::LuaCodec()
{
    _buff_len    = 0;
    _buff_pos    = 0;
    _decode_buff = nullptr;

    _encode_buff_len = MIN_BUFF;
    _encode_buff     = MIN_BUFF > 0 ? new char[MIN_BUFF] : nullptr;
}

LuaCodec::~LuaCodec()
{
    delete[] _encode_buff;
}

int32_t LuaCodec::decode_table(lua_State *L)
{
    size_t count = 0;
    // 这里不检测缓冲区，由decode_value检测
    *this >> count;

    if (!lua_checkstack(L, 3))
    {
        ELOG("lua stack overflow, top = %d", lua_gettop(L));
        return -1;
    }
    int32_t top = lua_gettop(L);

    lua_createtable(L, 0, 0);
    for (size_t i = 0; i < count; i++)
    {
        if (decode_value(L) < 0 || decode_value(L) < 0)
        {
            lua_settop(L, top);
            return -1;
        }
        lua_rawset(L, -3);
    }

    return 0;
}

void LuaCodec::check_decode_buff(size_t size)
{
    if (unlikely(_buff_pos + size > _buff_len))
    {
        std::string str = "lua codec decode buff reach end, expect len";
        str += std::to_string(size);
        throw std::overflow_error(str);
    }
}

int32_t LuaCodec::decode_value(lua_State *L)
{
    int8_t type = 0;

    check_decode_buff(sizeof(int8_t));
    *this >> type;

    // 由encode和encode_table检测，这里不用每次都检测
    //    if (!lua_checkstack(L, 1))
    //    {
    //        ELOG("lua stack overflow, %d", lua_gettop(L));
    //        return -1;
    //    }

    switch (type)
    {
    case LT_NIL: lua_pushnil(L); break;
    case LT_BOOL:
    {
        int8_t b = 0;

        check_decode_buff(sizeof(int8_t));
        *this >> b;
        lua_pushboolean(L, b);
        break;
    }
    case LT_NUM:
    {
        double d = 0.;

        check_decode_buff(sizeof(double));
        *this >> d;
        lua_pushnumber(L, d);
        break;
    }
    case LT_STR:
    {
        size_t len = 0;
        *this >> len;

        check_decode_buff(len);
        lua_pushlstring(L, subtract(len), len);

        break;
    }
    case LT_INT:
    {
        int64_t i = 0;
        check_decode_buff(sizeof(int64_t));

        *this >> i;
        lua_pushinteger(L, i);

        break;
    }
    case LT_TABLE:
    {
        check_decode_buff(sizeof(size_t));

        return decode_table(L);
        break;
    }
    default: ELOG("uknow data type %d", (int32_t)type); return -1;
    }

    return 0;
}

int32_t LuaCodec::decode(lua_State *L)
{
    _buff_pos    = 0;
    _decode_buff = luaL_checklstring(L, 2, &_buff_len);

    uint8_t count = 0;
    if (_buff_len < sizeof(count))
    {
        return luaL_error(L, "invalid lua binary buffer");
    }
    *this >> count;

    if (!lua_checkstack(L, count))
    {
        return luaL_error(L, "lua stack overflow,");
    }

    bool ok = true;
    try
    {
        for (int32_t i = 0; i < count; i++)
        {
            if (decode_value(L) < 0) return -1;
        }
    }
    catch (const std::overflow_error &e)
    {
        ELOG("%s", e.what());
        ok = false;
    }
    if (unlikely(!ok)) return luaL_error(L, "unknow error");

    return count;
}

int32_t LuaCodec::encode_table(lua_State *L, int32_t index)
{
    // table的key数量
    char *p_count = _encode_buff + _buff_len;

    size_t count = 0;
    // 写入数量，只是占位。这里不检测长度，由encode_value检测
    *this << count;

    int32_t top = lua_gettop(L);

    lua_pushnil(L);
    while (lua_next(L, index))
    {
        // lua中用table作key是相当于用table的内存地址作key，传过去就不一样了，要注意
        //        if (LUA_TTABLE == lua_type(L, top + 1))
        //        {
        //            ELOG("table as key not support");
        //            return -1;
        //        }
        if (encode_value(L, top + 1) < 0 || encode_value(L, top + 2))
        {
            return -1;
        }

        count++;
        lua_pop(L, 1);
    }

    memcpy(p_count, &count, sizeof(count));
    return 0;
}

void LuaCodec::check_encode_buff(size_t size)
{
    if (likely(sizeof(int8_t) + size + _buff_len < _encode_buff_len)) return;

    if (sizeof(int8_t) + size + _buff_len >= MAX_BUFF)
    {
        throw std::overflow_error("lua codec encode buffer overflow");
    }

    // 正常来讲，rpc调用的参数都是比较小的，不会触发内存分配才对。
    // 即使是一些比较大的变量（如string），也应该仅分配一次
    do
    {
        _encode_buff_len = _encode_buff_len * 2;
    } while (sizeof(int8_t) + size + _buff_len >= _encode_buff_len);

    char *old = _encode_buff;
    _encode_buff = new char[_encode_buff_len];
    memcpy(_encode_buff, old, _buff_len);

    delete[] old;
}

int32_t LuaCodec::encode_value(lua_State *L, int32_t index)
{
    switch (lua_type(L, index))
    {
    case LUA_TNIL:
        check_encode_buff(8);
        *this << (int8_t)LT_NIL;
        break;
    case LUA_TBOOLEAN:
        check_encode_buff(8);
        *this << (int8_t)LT_BOOL << (int8_t)(lua_toboolean(L, index) ? 1 : 0);
        break;
    case LUA_TNUMBER:
        check_encode_buff(8);
        if (lua_isinteger(L, index))
        {
            *this << (int8_t)LT_INT << (int64_t)lua_tointeger(L, index);
        }
        else
        {
            *this << (int8_t)LT_NUM << (double)lua_tonumber(L, index);
        }
        break;
    case LUA_TSTRING:
    {
        size_t len      = 0;
        const char *str = lua_tolstring(L, index, &len);

        check_encode_buff(len);
        // TODO 字符串长度用int32是否会好一点？好像也节省不了多少
        *this << (int8_t)LT_STR << (size_t)len;
        this->append(str, len);
        break;
    }
    case LUA_TTABLE:
        check_encode_buff(16);
        *this << (int8_t)LT_TABLE;
        return encode_table(L, index);
    default:
        ELOG("unsupport lua type %s", lua_typename(L, lua_type(L, index)));
        return -1;
    }

    return 0;
}

int32_t LuaCodec::encode(lua_State *L)
{
    int32_t index = 1;
    int top = lua_gettop(L);
    if (index > top || top - index > MAX_VARIABLE)
    {
        return luaL_error(L, 
            "invalid stack index or too many variable %I - %I", index, top);
    }

    _buff_len = 0;

    // 写入数量
    *this << uint8_t(top - index);

    bool ok = true;
    try
    {
        for (int32_t i = index + 1; i <= top; i++)
        {
            if (encode_value(L, i) < 0) return -1;
        }
    }
    catch (const std::overflow_error& e)
    {
        ELOG("%s", e.what());
        ok = false;
    }
    if (unlikely(!ok)) return luaL_error(L, "unknow error");

    lua_pushlstring(L, _encode_buff, _buff_len);
    return 1;
}
