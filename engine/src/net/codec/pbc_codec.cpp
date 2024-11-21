#include <vector>

#include "pbc_codec.hpp"

// pbc解码时，用于回调的结构
struct DecodeCtx
{
    int id;
    lua_State *L;
    class PbcCodec *pc;
};

void pbc_deleter(struct pbc_env *env)
{
    // 最后一个线程删除时，env最终失效。全局也必须重置，免得被新线程赋值
    // 虽然一般情况下最后一个线程失效时就只能是关服

    pbc_delete(env);
}

/**
 * pbc解码回调。每解码一个字段，都会回调这个函数
 * 参考pbc binding/lua53/pbc-lua53.c
 */
static void pbc_decode_cb(void *ud, int32_t type, const char *schema,
                          union pbc_value *v, int id, const char *key)
{
    // pbc是用c写的，只能这样回调，并且如果出错，无返回值...，不能中止解析过程
    DecodeCtx *ctx = (DecodeCtx *)ud;
    return ctx->pc->decode_cb(ctx, type, schema, v, id, key);
}

PbcCodec::PbcCodec()
{
    _write_msg = nullptr;
}

PbcCodec::~PbcCodec()
{
    _env.reset();
    // 最后一个线程释放时，直接释放全局内存，因此非关服情况下应该长久持有
    if (_g_env && 1 == _g_env.use_count()) _g_env.reset();
}

void PbcCodec::reset()
{
    _g_env = std::shared_ptr<struct pbc_env>(pbc_new(), pbc_deleter);
    _env   = _g_env;
}

void PbcCodec::update()
{
    _env = _g_env;
}

const char *PbcCodec::last_error()
{
    const char *env_e = pbc_error(_env.get());
    if (env_e && strlen(env_e) > 0)
    {
        _error_msg.append("\n").append(env_e);
    }
    _error_msg.append(" @ ");

    std::vector<std::string>::reverse_iterator rit = _trace_back.rbegin();
    while (rit != _trace_back.rend())
    {

        _error_msg.append("[").append(*rit).append("]");

        rit++;
    }

    return _error_msg.c_str();
}

int32_t PbcCodec::load(const char *buffer, size_t len)
{
    struct pbc_slice slice;
    slice.len    = (int)len;
    slice.buffer = const_cast<char *>(buffer);

    if (0 != pbc_register(_env.get(), &slice))
    {
        delete (char *)slice.buffer;
        ELOG("pbc register error:%s", pbc_error(_env.get()));
        return -1;
    }

    return 0;
}

void PbcCodec::clear_last()
{
    if (_write_msg)
    {
        pbc_wmessage_delete(_write_msg);
        _write_msg = nullptr;
    }

    _error_msg.clear();
    _trace_back.clear();
}

int32_t PbcCodec::decode_field(lua_State *L, int type, const char *schema,
                              union pbc_value *v)
{
    switch (type)
    {
    case PBC_FIXED32: lua_pushinteger(L, (uint32_t)v->i.low); break;
    case PBC_INT: lua_pushinteger(L, (int32_t)v->i.low); break;
    case PBC_REAL: lua_pushnumber(L, v->f); break;
    case PBC_BOOL: lua_pushboolean(L, v->i.low); break;
    case PBC_ENUM: lua_pushstring(L, v->e.name); break;
    case PBC_BYTES:
    case PBC_STRING:
        lua_pushlstring(L, (const char *)v->s.buffer, v->s.len);
        break;
    case PBC_MESSAGE: return decode_message(L, schema, &(v->s));
    case PBC_FIXED64:
    case PBC_UINT:
    case PBC_INT64:
    {
        // TODO 由于需要用位移，需要用uint64_t，由于lua不支持uint64_t，所以fixed64也是按int64_t来处理
        uint64_t v64 = (uint64_t)(v->i.hi) << 32 | (uint64_t)(v->i.low);
        lua_pushinteger(L, v64);
        break;
    }
    default:
        // 由于pbc解析过程无法中止，这里返回错误也没什么意义
        ELOG("protobuf unknown type %s", schema);
        break;
    }

    return 0;
}

void PbcCodec::decode_cb(DecodeCtx *ctx, int32_t type, const char *schema,
                         union pbc_value *v, int id, const char *key)
{
    // undefined field
    if (NULL == key)
    {
        return;
    }

    /**
     * pbc做这个decode时，如果是数组，在数组结束、开始时都没有回调的
     * 它是为惰性解码设计的，即每次解析只解析当前这一层的message
     * 因此不用考虑数组嵌套的问题。但我们这里是一次性全部解析的
     * message sub { repeated int list = 1; }
     * message test {
     *     repeated int list = 1;
     *     repeated sub sub_list = 2;
     *     repeated int sub_list_2 = 3;
     * }
     * 这种情况，如果id不一样，则不在同一个数组，同时需要结束上一个数组
     */
    lua_State *L = ctx->L;
    if (type & PBC_REPEATED)
    {
        if (id != ctx->id)
        {
            if (ctx->id)
            {
                lua_rawset(L, -3);
            }
            ctx->id = id;
            lua_pushstring(L, key);
            lua_newtable(L);
        }
        decode_field(L, type & ~PBC_REPEATED, schema, v);
        lua_rawseti(L, -2, lua_rawlen(L, -2) + 1);
    }
    else
    {
        // 上一个数组结束了
        if (ctx->id)
        {
            ctx->id = 0;
            lua_rawset(L, -3);
        }
        decode_field(L, type, schema, v);
        lua_setfield(L, -2, key);
    }
}

int32_t PbcCodec::decode_message(lua_State *L, const char *schema,
                                  struct pbc_slice *slice)
{
    if (!lua_checkstack(L, 3))
    {
        _error_msg = "protobuf decode stack overflow:";
        _error_msg += schema;
        return -1;
    }

    // 每解析一个子message，都需要一个ctx，所以这里只能是用栈变量，不能放成员变量
    DecodeCtx ctx;
    ctx.L   = L;
    ctx.id  = 0;
    ctx.pc = this;

    lua_newtable(L);

    int32_t ok = pbc_decode(_env.get(), schema, slice, pbc_decode_cb, &ctx);
    // 结束上一个数组
    if (ok && ctx.id)
    {
        lua_rawset(L, -3);
    }

    return ok;
}

int32_t PbcCodec::encode_field_list(lua_State *L, struct pbc_wmessage *wmsg,
                                int32_t type, int32_t index, const char *key,
                                const char *schema)
{
    if (type & PBC_REPEATED)
    {
        if (!lua_istable(L, index))
        {
            _error_msg = STD_FMT("field(%s) expect a table", key);
            return -1;
        }

        int32_t raw_type = (type & ~PBC_REPEATED);
        lua_pushnil(L);
        while (lua_next(L, index))
        {
            if (encode_field(L, wmsg, raw_type, index + 2, key, schema) < 0)
            {
                return -1;
            }

            lua_pop(L, 1);
        }

        return 0;
    }

    return encode_field(L, wmsg, type, index, key, schema);
}

int32_t PbcCodec::encode_field(lua_State *L, struct pbc_wmessage *wmsg,
                                    int32_t type, int32_t index,
                                    const char *key, const char *schema)
{
#define LUAL_CHECK(TYPE)                                               \
    if (!lua_is##TYPE(L, index))                                       \
    {                                                                  \
        _trace_back.push_back(key);                                    \
        _error_msg = STD_FMT("field(%s) expect " #TYPE ",got %s", key, \
                             lua_typename(L, lua_type(L, index)));     \
        return -1;                                                     \
    }

    switch (type)
    {
    case PBC_INT:
    case PBC_FIXED32:
    case PBC_UINT:
    case PBC_FIXED64:
    case PBC_INT64:
    {
        LUAL_CHECK(integer)
        int64_t val = (int64_t)lua_tointeger(L, index);
        uint32_t hi = (uint32_t)(val >> 32);
        pbc_wmessage_integer(wmsg, key, (uint32_t)val, hi);
    }
    break;
    case PBC_REAL:
    {
        LUAL_CHECK(number)
        double val = lua_tonumber(L, index);
        pbc_wmessage_real(wmsg, key, val);
    }
    break;
    case PBC_BOOL:
    {
        int32_t val = lua_toboolean(L, index);
        pbc_wmessage_integer(wmsg, key, (uint32_t)val, 0);
    }
    break;
    case PBC_ENUM:
    {
        LUAL_CHECK(integer)
        int32_t val = (int32_t)lua_tointeger(L, index);
        pbc_wmessage_integer(wmsg, key, (uint32_t)val, 0);
    }
    break;
    case PBC_STRING:
    case PBC_BYTES:
    {
        LUAL_CHECK(string)
        size_t len      = 0;
        const char *val = lua_tolstring(L, index, &len);
        if (pbc_wmessage_string(wmsg, key, val, (int32_t)len))
        {
            _error_msg = STD_FMT("field(%s) write string error", key);
            return -1;
        }
    }
    break;
    case PBC_MESSAGE:
    {
        LUAL_CHECK(table)
        struct pbc_wmessage *submsg = pbc_wmessage_message(wmsg, key);
        return encode_message(L, submsg, schema, index);
    }
    break;
    default: _error_msg = STD_FMT("protobuf unknow type: %d", type); return -1;
    }
    return 0;

#undef LUAL_CHECK
}

int32_t PbcCodec::encode_message(lua_State *L, struct pbc_wmessage *wmsg,
                              const char *schema, int32_t index)
{
    if (!lua_istable(L, index))
    {
        _error_msg = STD_FMT("protobuf encode expect a table at %d", index);
        return -1;
    }

    if (!lua_checkstack(L, 2))
    {
        _error_msg = STD_FMT("protobuf encode stack overflow: %d", lua_gettop(L));
        return -1;
    }

    int32_t top = lua_gettop(L);
    pbc_env *env = _env.get();

    /* pbc并未提供遍历sdl的方法，只能反过来遍历tabale.
     * 如果table中包含较多的无效字段，hash消耗将会比较大
     */
    lua_pushnil(L);
    while (lua_next(L, index))
    {
        /* field name can only be string,if not,ignore */
        if (LUA_TSTRING != lua_type(L, -2))
        {
            lua_pop(L, 1);
            continue;
        }

        const char *key = lua_tostring(L, -2);

        const char *sub_object = NULL;
        int32_t val_type       = pbc_type(env, schema, key, &sub_object);
        if (val_type <= 0)
        {
            lua_pop(L, 1);
            continue;
        }

        if (encode_field_list(L, wmsg, val_type, top + 2, key, sub_object) < 0)
        {
            _trace_back.push_back(schema);
            return -1;
        }
        lua_pop(L, 1);
    }
    return 0;
}

int32_t PbcCodec::decode(lua_State *L)
{
    assert(_env);
    const char *schema = luaL_checkstring(L, 2);
    
    size_t size = 0;
    const char *buffer = luaL_checklstring(L, 3, &size);

    clear_last();

    struct pbc_slice slice;
    slice.len    = static_cast<int32_t>(size);
    slice.buffer = const_cast<char *>(buffer);

    /**
     * 以前用struct pbc_rmessage *msg = pbc_rmessage_new(_env, schema, &slice);
     * 来解析，然后用pbc_rmessage_next来遍历解析出的字段再赋值给lua
     * 这种方式相当于把所有字段先解析放到一个hash表，再一个个取出来赋值给lua
     * 而pbc_decode的方式则是在解析过程中直接放到lua表，稍微快一些
     */

    if (decode_message(L, schema, &slice) < 0)
    {
        return luaL_error(L, "%s", last_error());
    }

    // 默认情况下，所有内容解析到一个table
    return 1;
}

int32_t PbcCodec::encode(lua_State *L)
{
    assert(_env);
    clear_last();

    const char *schema = luaL_checkstring(L, 2);

     _write_msg = pbc_wmessage_new(_env.get(), schema);
    if (!_write_msg)
    {
        _error_msg = STD_FMT("no such protobuf message found: %s", schema);
        return 0;
    }

    if (encode_message(L, _write_msg, schema, 3) < 0)
    {
        return luaL_error(L, "%s", last_error());
    }

    struct pbc_slice slice;
    pbc_wmessage_buffer(_write_msg, &slice);

    lua_pushlstring(L, (const char *)slice.buffer, slice.len);
    return 1;
}
