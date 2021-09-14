#pragma once

// lua与bson的交互，这个文件暂时只有mongodb在用，所以所有逻辑都放头文件了

#include <bson.h>
#include "ltools.hpp" // lua_isbit32

static const char *ARRAY_OPT    = "__array_opt";
static const int32_t MAX_STACK_DEEP = 128;
static const int32_t MAX_KEY_LENGTH = 128;

/// 把一个lua value转换为bson的字段
static int32_t encode_value(lua_State *L, int32_t index, bson_t *b,
                            const char *key, int32_t key_len, bson_error_t *e,
                            double array_opt);

/// 把一个bson转换为lua value
static int decode_value(lua_State *L, bson_iter_t *iter, bson_error_t *e,
                        double array_opt);

static void destory_bson_list() {}

static void destory_bson_list(bson_t *b)
{
    bson_destroy(b);
}

template <typename... Args> void destory_bson_list(bson_t *b, Args... args)
{
    destory_bson_list(b);
    destory_bson_list(args...);
}

/**
 * check a lua table is object or array
 * @return object = 0, max_index = 1 if using array_opt
 *         array = 1, max_index is max array index
 */
static int32_t check_type(lua_State *L, int32_t index, lua_Integer *max_index, double opt)
{
    lua_Integer key     = 0;
    int32_t key_count       = 0;
    double index_opt    = 0.;
    double percent_opt  = 0.;
    lua_Integer max_key = 0;
#if LUA_VERSION_NUM < 503
    double num_key = 0.;
#endif

    if (luaL_getmetafield(L, index, ARRAY_OPT))
    {
        if (LUA_TNUMBER == lua_type(L, -1)) opt = lua_tonumber(L, -1);

        lua_pop(L, 1); /* pop metafield value */
    }

    // encode as object, using array_opt
    if (0. == opt)
    {
        *max_index = 1;
        return 0;
    }

    if (opt > 1) percent_opt = modf(opt, &index_opt);

    // try to find out the max key
    lua_pushnil(L);
    while (lua_next(L, index) != 0)
    {
        /* stack status:table, key, value */
#if LUA_VERSION_NUM >= 503 // lua 5.3 has int64
        if (!lua_isinteger(L, -2))
        {
            goto NO_MAX_KEY;
        }
        key = lua_tointeger(L, -2);
#else
        if (lua_type(L, -2) != LUA_TNUMBER)
        {
            goto NO_MAX_KEY;
        }
        num_key = lua_tonumber(L, -2);
        key     = floor(num_key);
        /* array index must be interger and >= 1 */
        if (key < 1 || key != num_key)
        {
            goto NO_MAX_KEY;
        }
#endif
        key_count++;
        if (key > max_key) max_key = key;

        lua_pop(L, 1);
    }

    if (opt != 1.0)
    {
        // empty table decode as object
        if (0 == max_key) return 0;

        // max_key larger than array_opt integer part and then item count fail
        // to fill the percent of array_opt fractional part, encode as object
        if (opt > 0. && max_key > (lua_Integer)index_opt
            && ((double)key_count) / (double)max_key < percent_opt)
        {
            *max_index = 1;
            return 0;
        }
    }

    *max_index = max_key;
    return 1;

NO_MAX_KEY:
    lua_pop(L, 2);
    if (1. == opt)
    {
        // force encode as array, no max key found
        *max_index = -1;
        return 1;
    }
    else
    {
        // none integer key found, encode as object
        if (opt >= 0) *max_index = 1;
        return 0;
    }
}

/**
 * 把lua table构建成bson array对象
 */
static int32_t encode_array(bson_t *b, lua_State *L, int32_t index, int32_t max_index,
                             bson_error_t *e, double array_opt)
{
    const char *key;
    char key_buff[MAX_KEY_LENGTH];

    int32_t top = lua_gettop(L);

    /* lua array start from 1 */
    for (int bson_index = 0; bson_index < max_index; bson_index++)
    {
        // If value is from 0 to 999, it will use a constant string in the data section of the library
        size_t key_len = bson_uint32_to_string(bson_index, &key, key_buff, sizeof(key_buff));

        lua_rawgeti(L, index, bson_index + 1);
        if (encode_value(L, top + 1, b, key, (int32_t)key_len, e, array_opt) < 0)
        {
            lua_pop(L, 1);

            return -1;
        }

        lua_pop(L, 1);
    }
    return 0;
}

/**
 * 把lua table构建成bson object对象
 */
static int32_t encode_invalid_key_array(bson_t *b, lua_State *L, int32_t index,
                                        bson_error_t *e, double array_opt)
{
    const char *key;
    int bson_index           = 0;
    char key_buff[MAX_KEY_LENGTH];

    int32_t top = lua_gettop(L);

    // key中包含字符串、浮点等，只能忽略所有key，遍历构建数组，顺序是无法保证的
    lua_pushnil(L);
    while (lua_next(L, index) != 0)
    {
        size_t key_len =
            bson_uint32_to_string(bson_index++, &key, key_buff, sizeof(key_buff));
        if (encode_value(L, top + 2, b, key, (int32_t)key_len, e, array_opt) < 0)
        {
            lua_pop(L, 2);

            return -1;
        }

        lua_pop(L, 1);
    }

    return 0;
}

/**
 * 把lua table构建成bson object对象
 */
static int32_t encode_object(bson_t *b, lua_State *L, int32_t index,
                             bson_error_t *e, double array_opt)
{
    char key_buff[MAX_KEY_LENGTH] = {0};

    int32_t top = lua_gettop(L);

    lua_pushnil(L); /* first key */
    while (lua_next(L, index) != 0)
    {
        int32_t key_len     = -1;
        const char *key = nullptr;
        switch (lua_type(L, -2))
        {
        case LUA_TBOOLEAN:
        {
            key = lua_toboolean(L, -2) ? "true" : "false";
            key_len = (int32_t)strlen(key);
        }
        break;
        case LUA_TNUMBER:
        {
            if (lua_isinteger(L, -2))
            {
                snprintf(key_buff, MAX_KEY_LENGTH, LUA_INTEGER_FMT,
                         lua_tointeger(L, -2));
            }
            else
            {
                snprintf(key_buff, MAX_KEY_LENGTH, LUA_NUMBER_FMT,
                         lua_tonumber(L, -2));
            }
            key = key_buff;
        }
        break;
        case LUA_TSTRING:
        {
            size_t len = 0;
            key        = lua_tolstring(L, -2, &len);
            if (len > MAX_KEY_LENGTH - 1)
            {
                lua_pop(L, 2);
                bson_set_error(e, 0, 0, "lua table string key too long");
                return -1;
            }
            key_len = (int32_t)len;
        }
        break;
        default:
        {
            lua_pop(L, 2);
            bson_set_error(e, 0, 0, "can not convert %s to bson key",
                           lua_typename(L, lua_type(L, -2)));
            return -1;
        }
        break;
        }

        assert(key);
        if (encode_value(L, top + 2, b, key, key_len, e, array_opt) < 0)
        {
            lua_pop(L, 2);
            return -1;
        }

        /* removes 'value'; keep 'key' for next iteration */
        lua_pop(L, 1);
    }

    return 0;
}

/**
 * 把lua table构建成bson对象
 */
static int32_t encode_table(lua_State *L, int32_t index, bson_t *parent, const char *key, int32_t key_len, bson_error_t *e,
                            double array_opt)
{
    if (lua_gettop(L) > MAX_STACK_DEEP)
    {
        bson_set_error(e, 0, 0, "over MAX_STACK_DEEP");
        return -1;
    }

    if (!lua_checkstack(L, 2))
    {
        bson_set_error(e, 0, 0, "stack overflow");
        return -1;
    }

    if (!lua_istable(L, index))
    {
        bson_set_error(e, 0, 0, "table expect, got %s", lua_typename(L, lua_type(L, index)));
        return -1;
    }

    bson_t b;
    int32_t ok            = 0;
    lua_Integer max_index = -1;
    int32_t type = check_type(L, index, &max_index, array_opt);
    if (0 == type)
    {
        // child MUST be an uninitialized bson_t to avoid leaking memory
        bson_append_document_begin(parent, key, key_len, &b);

        ok = encode_object(&b, L, index, e, array_opt);
        // append `__array_opt` to object if using array_opt
        if (1 == max_index && ok)
        {
            bson_append_int32(&b, ARRAY_OPT, (int32_t)strlen(ARRAY_OPT), 0);
        }
        bson_append_document_end(parent, &b);
    }
    else
    {

        bson_append_array_begin(parent, key, key_len, &b);
        ok = max_index > 0
                 ? encode_array(&b, L, index, (int32_t)max_index, e, array_opt)
                 : encode_invalid_key_array(&b, L, index, e, array_opt);
        bson_append_array_end(parent, &b);
    }

    return ok;
}

static int32_t encode_value(lua_State *L, int32_t index, bson_t *b, const char *key,
                     int32_t key_len, bson_error_t *e, double array_opt)
{
    switch (lua_type(L, index))
    {
    case LUA_TNIL:
    {
        bson_append_null(b, key, key_len);
    }
    break;
    case LUA_TBOOLEAN:
    {
        bson_append_bool(b, key, key_len, lua_toboolean(L, index));
    }
    break;
    case LUA_TNUMBER:
    {
        if (lua_isinteger(L, index))
        {
            /* int32 or int64 */
            lua_Integer val = lua_tointeger(L, index);
            if (lua_isbit32(val))
                bson_append_int32(b, key, key_len, (int32_t)val);
            else
                bson_append_int64(b, key, key_len, val);
        }
        else
        {
            bson_append_double(b, key, key_len, lua_tonumber(L, index));
        }
    }
    break;
    case LUA_TSTRING:
    {
        size_t len      = 0;
        const char *val = lua_tolstring(L, index, &len);
        bson_append_utf8(b, key, (int32_t)key_len, val, (int32_t)len);
    }
    break;
    case LUA_TTABLE:
    {
        return encode_table(L, index, b, key, key_len, e, array_opt);
    }
    break;
    default:
    {
        bson_set_error(e, 0, 0,
            "can not convert type %s to bson value",
            lua_typename(L, lua_type(L, index)));
        return -1;
    }
    break;
    }

    return 0;
}

/// @brief 把lua table构建成bson对象
static bson_t *encode(lua_State *L, int index, bson_error_t *e, double array_opt)
{
    if (lua_gettop(L) > MAX_STACK_DEEP)
    {
        bson_set_error(e, 0, 0, "over MAX_STACK_DEEP");
        return nullptr;
    }

    if (!lua_checkstack(L, 2))
    {
        bson_set_error(e, 0, 0, "stack overflow");
        return nullptr;
    }

    if (!lua_istable(L, index))
    {
        bson_set_error(e, 0, 0, "table expect, got %s", lua_typename(L, lua_type(L, index)));
        return nullptr;
    }

    bson_t *b = bson_new();

    int32_t ok            = 0;
    lua_Integer max_index = -1;
    int32_t type          = check_type(L, index, &max_index, array_opt);
    if (0 == type)
    {
        ok = encode_object(b, L, index, e, array_opt);
        // append `__array_opt` to object if using array_opt
        if (1 == max_index && 0 == ok)
        {
            bson_append_int32(b, ARRAY_OPT, (int32_t)strlen(ARRAY_OPT), 0);
        }
    }
    else
    {
        ok = max_index > 0
                 ? encode_array(b, L, index, (int32_t)max_index, e, array_opt)
                           : encode_invalid_key_array(b, L, index, e, array_opt);
    }
    if (0 != ok)
    {
        bson_destroy(b);
        b = nullptr;
    }

    return b;
}

/// @brief 判断是否需要转换数字key
/// @param b bson对象
/// @param iter bson迭代器
/// @param type bson类型
/// @param array_opt 是否启用转换
/// @return 是否需要转换数字key
static bool is_convert_key(bson_t *b, bson_iter_t *iter, bson_type_t type,
                          double array_opt)
{
    if (array_opt < 0 || BSON_TYPE_DOCUMENT != type) return false;

    bson_iter_t finder;
    if (b) return bson_iter_init_find(&finder, b, ARRAY_OPT);
    if (iter)
    {
        return bson_iter_recurse(iter, &finder)
               && bson_iter_find(&finder, ARRAY_OPT);
    }

    return false;
}

/// @brief 把bson对象解析为lua table
/// @param L Lua虚拟机
/// @param iter bson迭代器
/// @param e bson错误对象
/// @param type bson类型，如 BSON_TYPE_ARRAY
/// @param array_opt 是否启用数组自动转换,-1表示不启用
/// @param convert 是否执行key转换
/// @return <0表示失败
static int decode_table(lua_State *L, bson_iter_t *iter, bson_error_t *e,
                        bson_type_t type, double array_opt, bool convert)
{
    // 对于bson而言，只有object类型(即bson的document)，没有数组，数组只是key为"0"、"1"、...的object
    // https://groups.google.com/g/bson/c/VHaO42PPMGc/m/l-ZqIMcLpfMJ
    // 因此，解析一个顶层的bson时，只能由外部指定是解析为object还是array

    if (!lua_checkstack(L, 3))
    {
        bson_set_error(e, 0, 0, "decode_table stack overflow");
        return -1;
    }

    lua_newtable(L);
    while (bson_iter_next(iter))
    {
        const char *key = bson_iter_key(iter);
        if (decode_value(L, iter, e, array_opt) < 0)
        {
            lua_pop(L, 1);
            return -1;
        }

        if (BSON_TYPE_ARRAY == type)
        {
            // lua array index start from 1
            lua_seti(L, -2, strtol(key, nullptr, 10) + 1);
        }
        else if (convert)
        {
            char *end_ptr      = nullptr;
            long long int ikey = strtoll(key, &end_ptr, 10);
            // if can not convert to integer, push the string key
            if (0 == ikey && *end_ptr != '\0')
                lua_pushstring(L, key);
            else
                lua_pushinteger(L, ikey);
        }
        else
        {
            lua_setfield(L, -2, key);
        }
    }

    return 0;
}

static int decode_value(lua_State *L, bson_iter_t *iter, bson_error_t *e,
                        double array_opt)
{
    switch (bson_iter_type(iter))
    {
    case BSON_TYPE_DOUBLE:
    {
        double val = bson_iter_double(iter);
        lua_pushnumber(L, val);
    }
    break;
    case BSON_TYPE_DOCUMENT:
    {
        bson_iter_t sub_iter;
        if (!bson_iter_recurse(iter, &sub_iter))
        {
            bson_set_error(e, 0, 0, "bson document iter recurse error");
            return -1;
        }
        bool convert  =
            is_convert_key(nullptr, iter, BSON_TYPE_DOCUMENT, array_opt);
        if (decode_table(L, &sub_iter, e, BSON_TYPE_DOCUMENT, array_opt, convert) < 0)
        {
            return -1;
        }
    }
    break;
    case BSON_TYPE_ARRAY:
    {
        bson_iter_t sub_iter;
        if (!bson_iter_recurse(iter, &sub_iter))
        {
            bson_set_error(e, 0, 0, "bson array iter recurse error");
            return -1;
        }
        if (decode_table(L, &sub_iter, e, BSON_TYPE_ARRAY, array_opt, false) < 0)
        {
            return -1;
        }
    }
    break;
    case BSON_TYPE_BINARY:
    {
        const char *val  = nullptr;
        unsigned int len = 0;
        bson_iter_binary(iter, nullptr, &len, (const uint8_t **)(&val));
        lua_pushlstring(L, val, len);
    }
    break;
    case BSON_TYPE_UTF8:
    {
        unsigned int len = 0;
        const char *val  = bson_iter_utf8(iter, &len);
        lua_pushlstring(L, val, len);
    }
    break;
    case BSON_TYPE_OID:
    {
        const bson_oid_t *oid = bson_iter_oid(iter);

        char str[25]; /* bson api make it 25 */
        bson_oid_to_string(oid, str);
        lua_pushstring(L, str);
    }
    break;
    case BSON_TYPE_BOOL:
    {
        bool val = bson_iter_bool(iter);
        lua_pushboolean(L, val);
    }
    break;
    case BSON_TYPE_NULL:
    {
        /* nullptr == nil in lua */
        lua_pushnil(L);
    }
    break;
    case BSON_TYPE_INT32:
    {
        int val = bson_iter_int32(iter);
        lua_pushinteger(L, val);
    }
    break;
    case BSON_TYPE_DATE_TIME:
    {
        /* A 64-bit integer containing the number of milliseconds since
         * the UNIX epoch
         */
        int64_t val = bson_iter_date_time(iter);
        lua_pushinteger(L, val);
    }
    break;
    case BSON_TYPE_INT64:
    {
        int64_t val = bson_iter_int64(iter);
        lua_pushinteger(L, val);
    }
    break;
    default:
    {
        bson_set_error(e, 0, 0, "unknow bson type:%d", bson_iter_type(iter));
        return -1;
    }
    break;
    }

    return 0;
}

/// @brief 解析bson对象到lua table
/// @param type 指定bson的类型，如 BSON_TYPE_ARRAY
/// @param array_opt 是否启用数组自动转换,-1表示不启用
static int decode(lua_State *L, bson_t *b, bson_error_t *e,
                  bson_type_t type, double array_opt)
{
    bson_iter_t iter;
    if (!bson_iter_init(&iter, b))
    {
        bson_set_error(e, 0, 0, "invalid bson document to decode");

        return -1;
    }

    return decode_table(L, &iter, e, type, array_opt, is_convert_key(b, nullptr, type, array_opt));
}

/**
 * 从lua堆栈构建一个bson对象。如果构建失败，此函数为执行luaL_error
 * @param L lua虚拟机
 * @param index 堆栈索引
 * @param def 如果为空，是否创建一个默认bson对象
 * @param args 需要如果构建失败，需要删除的bson对象
 * @return bson对象，可能为nullptr
 */
template <typename... Args>
bson_t *bson_new_from_lua(lua_State *L, int32_t index, int32_t def, double array_opt,
                          Args... args)
{
    bson_t *bson = nullptr;
    if (lua_istable(L, index)) // 自动将lua table 转化为bson
    {
        bson_error_t e;
        if (!(bson = encode(L, index, &e, array_opt)))
        {
            destory_bson_list(args...);
            luaL_error(L, "table to bson error: %s", e.message);
            return nullptr;
        }

        return bson;
    }
    else if (lua_isstring(L, index)) // json字符串
    {
        const char *json = lua_tostring(L, index);
        bson_error_t e;
        bson =
            bson_new_from_json(reinterpret_cast<const uint8_t *>(json), -1, &e);
        if (!bson)
        {
            destory_bson_list(args...);
            luaL_error(L, "json to bson error: %s", e.message);
            return nullptr;
        }

        return bson;
    }

    if (0 == def) return nullptr;
    if (1 == def) return bson_new();

    luaL_error(L, "argument #%d expect table or json string", index);
    return nullptr;
}
