#include <vector>

#include "../net_header.hpp"
#include <lua.hpp>
#include <pbc.h>

#include <fstream>
#include <filesystem>

#include "protobuf_codec.hpp"

/* check if suffix match */
static int is_suffix_file(const char *path, const char *suffix)
{
    /* simply check,not consider file like ./subdir/.bfbs */
    size_t sz = strlen(suffix);
    size_t ps = strlen(path);

    /* file like .pb will be ignore */
    if (ps <= sz + 2) return 0;

    if ('.' == path[ps - sz - 1] && 0 == strcmp(path + ps - sz, suffix))
    {
        return true;
    }

    return false;
}

/**
 * 对pbc再进行一层包装用于C++
 */
class lprotobuf
{
public:
    /**
     * 解码时，用于回pbc回调的结构
     */
    struct DecodeCtx
    {
        int id;
        lua_State *L;
        class lprotobuf *lpb;
    };

public:
    ~lprotobuf();
    lprotobuf();

    void reset();

    int32_t encode(lua_State *L, const char *object, int32_t index);
    int32_t decode(lua_State *L, const char *object, const char *buffer,
                   size_t size);

    const char *last_error();
    void get_buffer(struct pbc_slice &slice);

    int32_t load_file(const char *paeth);
    int32_t load_path(const char *path, const char *suffix = "pb");

    static void decode_cb(void *ud, int32_t type, const char *object,
                          union pbc_value *v, int id, const char *key);

private:
    int32_t raw_encode(lua_State *L, struct pbc_wmessage *wmsg,
                       const char *object, int32_t index);
    int32_t encode_field(lua_State *L, struct pbc_wmessage *wmsg, int32_t type,
                         int32_t index, const char *key, const char *object);
    int32_t raw_encode_field(lua_State *L, struct pbc_wmessage *wmsg,
                             int32_t type, int32_t index, const char *key,
                             const char *object);

    int32_t decode_message(lua_State *L, const char *object,
                           struct pbc_slice *slice);

    void raw_decode_cb(struct DecodeCtx *ctx, int32_t type, const char *object,
                       union pbc_value *v, int id, const char *key);

    int32_t push_value(lua_State *L, int type, const char *object,
                       union pbc_value *v);

private:
    struct pbc_env *_env;
    struct pbc_wmessage *_write_msg;

    std::string _error_msg;               // 错误信息
    std::vector<std::string> _trace_back; // 出错时，用于跟踪哪个字段有问题
};

lprotobuf::lprotobuf()
{
    _env       = pbc_new();
    _write_msg = NULL;
}

lprotobuf::~lprotobuf()
{
    reset();

    if (_env)
    {
        pbc_delete(_env);
    }
    _env = NULL;
}

void lprotobuf::reset()
{
    if (_write_msg)
    {
        pbc_wmessage_delete(_write_msg);
    }

    _write_msg = NULL;
    _error_msg.clear();
    _trace_back.clear();
}

int32_t lprotobuf::load_file(const char *path)
{
    std::ifstream ifs(path, std::ifstream::binary | std::ifstream::in);
    if (!ifs.good())
    {
        ELOG("can NOT open file(%s):%s", path, strerror(errno));
        return -1;
    }

    // get length of file:
    ifs.seekg(0, ifs.end);
    int32_t len = static_cast<int32_t>(ifs.tellg());
    ifs.seekg(0, ifs.beg);

    if (!ifs.good() || len <= 0)
    {
        ifs.close();
        ELOG("get file length error:%s", path);
        return -1;
    }

    struct pbc_slice slice;
    slice.len    = len;
    slice.buffer = new char[slice.len];
    ifs.read((char *)slice.buffer, slice.len);

    if (!ifs.good() || ifs.gcount() != slice.len)
    {
        ifs.close();
        delete (char *)slice.buffer;
        ELOG("read file content error:%s", path);
        return -1;
    }
    ifs.close();

    if (0 != pbc_register(_env, &slice))
    {
        delete (char *)slice.buffer;
        ELOG("pbc register error(%s):%s", path, pbc_error(_env));
        return -1;
    }

    delete[](char *) slice.buffer;
    return 0;
}

int32_t lprotobuf::load_path(const char *path, const char *suffix)
{
    char file_path[PATH_MAX];
    int sz = snprintf(file_path, PATH_MAX, "%s/", path);
    if (sz <= 0)
    {
        ELOG("path too long:%s", path);

        return -1;
    }

    int count         = 0;
    std::error_code e;
    std::filesystem::directory_iterator dir_iter(path, e);
    if (e)
    {
        ELOG("can not open directory(%s):%s", path, e.message());

        return -1;
    }
    for (auto &p : dir_iter)
    {
        if (!p.is_regular_file()) continue;

        const std::string s_path = p.path().string();
        if (!is_suffix_file(s_path.c_str(), suffix)) continue;

        if (load_file(s_path.c_str()) < 0)
        {
            return -1;
        }
        ++count;
    }

    return count;
}

const char *lprotobuf::last_error()
{
    const char *env_e = pbc_error(_env);
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

void lprotobuf::get_buffer(struct pbc_slice &slice)
{
    assert(_write_msg);

    pbc_wmessage_buffer(_write_msg, &slice);
}

int32_t lprotobuf::decode(lua_State *L, const char *object, const char *buffer,
                          size_t size)
{
    reset();

    struct pbc_slice slice;
    slice.len    = static_cast<int32_t>(size);
    slice.buffer = const_cast<char *>(buffer);

    /**
     * 以前用struct pbc_rmessage *msg = pbc_rmessage_new(_env, object, &slice);
     * 来解析，然后用pbc_rmessage_next来遍历解析出的字段再赋值给lua
     * 这种方式相当于把所有字段先解析放到一个hash表，再一个个取出来赋值给lua
     * 而pbc_decode的方式则是在解析过程中直接放到lua表，稍微快一些
     */

    return decode_message(L, object, &slice);
    ;
}

int32_t lprotobuf::decode_message(lua_State *L, const char *object,
                                  struct pbc_slice *slice)
{
    if (!lua_checkstack(L, 3))
    {
        _error_msg = STD_FMT("protobuf decode stack overflow:%s", object);
        return -1;
    }

    lprotobuf::DecodeCtx ctx;
    ctx.L   = L;
    ctx.id  = 0;
    ctx.lpb = this;

    lua_newtable(L);

    int32_t ok = pbc_decode(_env, object, slice, lprotobuf::decode_cb, &ctx);
    // 结束上一个数组
    if (ok && ctx.id)
    {
        lua_rawset(L, -3);
    }

    return ok;
}

/**
 * pbc解码回调。每解码一个字段，都会回调这个函数
 * 参考pbc binding/lua53/pbc-lua53.c
 */
void lprotobuf::decode_cb(void *ud, int32_t type, const char *object,
                          union pbc_value *v, int id, const char *key)
{
    // pbc是用c写的，只能这样回调，并且如果出错，无返回值...，不能中止解析过程
    lprotobuf::DecodeCtx *ctx = (lprotobuf::DecodeCtx *)ud;
    return ctx->lpb->raw_decode_cb(ctx, type, object, v, id, key);
}

void lprotobuf::raw_decode_cb(DecodeCtx *ctx, int32_t type, const char *object,
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
        push_value(L, type & ~PBC_REPEATED, object, v);
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
        push_value(L, type, object, v);
        lua_setfield(L, -2, key);
    }
}

int32_t lprotobuf::push_value(lua_State *L, int type, const char *object,
                              union pbc_value *v)
{
    switch (type)
    {
    case PBC_FIXED32:
    case PBC_INT: lua_pushinteger(L, (int)v->i.low); break;
    case PBC_REAL: lua_pushnumber(L, v->f); break;
    case PBC_BOOL: lua_pushboolean(L, v->i.low); break;
    case PBC_ENUM: lua_pushstring(L, v->e.name); break;
    case PBC_BYTES:
    case PBC_STRING:
        lua_pushlstring(L, (const char *)v->s.buffer, v->s.len);
        break;
    case PBC_MESSAGE: return decode_message(L, object, &(v->s));
    case PBC_FIXED64:
    case PBC_UINT:
    case PBC_INT64:
    {
        uint64_t v64 = (uint64_t)(v->i.hi) << 32 | (uint64_t)(v->i.low);
        lua_pushinteger(L, v64);
        break;
    }
    default:
        // 由于pbc解析过程无法中止，这里返回错误也没什么意义
        ELOG("protobuf unknown type %s", object);
        break;
    }

    return 0;
}

int32_t lprotobuf::encode(lua_State *L, const char *object, int32_t index)
{
    reset();

    assert(NULL == _write_msg);

    _write_msg = pbc_wmessage_new(_env, object);
    if (!_write_msg)
    {
        _error_msg = STD_FMT("no such protobuf message found: %s", object);
        return -1;
    }

    return raw_encode(L, _write_msg, object, index);
    ;
}

int32_t lprotobuf::encode_field(lua_State *L, struct pbc_wmessage *wmsg,
                                int32_t type, int32_t index, const char *key,
                                const char *object)
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
            if (raw_encode_field(L, wmsg, raw_type, index + 2, key, object) < 0)
            {
                return -1;
            }

            lua_pop(L, 1);
        }

        return 0;
    }

    return raw_encode_field(L, wmsg, type, index, key, object);
}

int32_t lprotobuf::raw_encode_field(lua_State *L, struct pbc_wmessage *wmsg,
                                    int32_t type, int32_t index,
                                    const char *key, const char *object)
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
        return raw_encode(L, submsg, object, index);
    }
    break;
    default: _error_msg = STD_FMT("protobuf unknow type: %d", type); return -1;
    }
    return 0;

#undef LUAL_CHECK
}

int32_t lprotobuf::raw_encode(lua_State *L, struct pbc_wmessage *wmsg,
                              const char *object, int32_t index)
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

    /* pbc并未提供遍历sdl的方法，只能反过来遍历tabale.
     * 如果table中包含较多的无效字段，hash消耗将会比较大
     */
    lua_pushnil(L);
    while (lua_next(L, index))
    {
        /* field name can only be string,if not,ignore */
        if (LUA_TSTRING != lua_type(L, -2)) continue;

        const char *key = lua_tostring(L, -2);

        const char *sub_object = NULL;
        int32_t val_type       = pbc_type(_env, object, key, &sub_object);
        if (val_type <= 0)
        {
            lua_pop(L, 1);
            continue;
        }

        if (encode_field(L, wmsg, val_type, top + 2, key, sub_object) < 0)
        {
            _trace_back.push_back(object);
            return -1;
        }
        lua_pop(L, 1);
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
ProtobufCodec::ProtobufCodec()
{
    _is_proto_loaded = false;
    _lprotobuf       = new class lprotobuf();
}

ProtobufCodec::~ProtobufCodec()
{
    delete _lprotobuf;
    _lprotobuf = NULL;
}

/**
 * 打包结束后，后续清理
 */
void ProtobufCodec::finalize()
{
    if (_lprotobuf)
    {
        _lprotobuf->reset();
    }
}

void ProtobufCodec::reset()
{
    delete _lprotobuf;
    _lprotobuf = new class lprotobuf();
}

int32_t ProtobufCodec::load_path(const char *path)
{
    // pbc并没有什么unregister之类的函数，只能把整个对象销毁重建了
    if (_is_proto_loaded) reset();

    _is_proto_loaded = true;
    return _lprotobuf->load_path(path);
}

int32_t ProtobufCodec::load_file(const char* path)
{
    return _lprotobuf->load_file(path);
}

/**
 * 解码数据包
 * @return: <0 error,otherwise the number of parameter push to stack
 */
int32_t ProtobufCodec::decode(lua_State *L, const char *buffer, size_t len,
                              const CmdCfg *cfg)
{
    if (_lprotobuf->decode(L, cfg->_object, buffer, len) < 0)
    {
        ELOG("protobuf decode:%s", _lprotobuf->last_error());
        return -1;
    }

    // 默认情况下，所有内容解析到一个table
    return 1;
}

/**
 * 编码数据包
 * @return: <0 error,otherwise the length of buffer
 */
int32_t ProtobufCodec::encode(lua_State *L, int32_t index, const char **buffer,
                              const CmdCfg *cfg)
{
    if (_lprotobuf->encode(L, cfg->_object, index) < 0)
    {
        ELOG("protobuf encode:%s", _lprotobuf->last_error());
        return -1;
    }

    struct pbc_slice slice;
    _lprotobuf->get_buffer(slice);

    *buffer = static_cast<const char *>(slice.buffer);

    return slice.len;
}
