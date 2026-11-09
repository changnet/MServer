#include <atomic>
#include <cassert>
#include <cstdio>
#include <vector>

#include "pbc_codec.hpp"

// 解码扁平快速路径的上下文，每层 message 一个栈变量
struct FastCtx
{
    lua_State *L;
    class PbcCodec *pc;
    int32_t result; // 本层结果表的绝对栈索引，map 用
    int id; // 当前 repeated 字段 id，0 表示没有正在写的数组
    int n;  // 当前数组已写入的元素数，避免每次 lua_rawlen
};

// 单个 MapEntry 的解码上下文：默认值先压栈，回调按字段 id 覆盖
struct MapEntryCtx
{
    lua_State *L;
    class PbcCodec *pc;
    int32_t key_idx; // key 默认值的绝对栈索引
    int32_t val_idx; // value 默认值的绝对栈索引
};

// core 已按字段类型解好值，这里只负责推到 Lua 栈上
static inline void push_scalar(lua_State *L, int32_t type,
                               const struct pbc_fvalue *v)
{
    switch (type)
    {
    case PBC_INT:
    case PBC_SFIXED32:
    case PBC_ENUM:
        /* int32 / sint32 / sfixed32 / enum 都是有符号 32 位。
         * enum 一律给整数（proto 的 enum 在 wire 上就是 varint），
         * 这样 decode 出来的值可以直接拿去 encode。 */
        lua_pushinteger(L, (lua_Integer)(int32_t)(uint32_t)v->i);
        break;
    case PBC_UINT32:
    case PBC_FIXED32:
        /* 无符号 32 位：0~4294967295 用 Lua 的 64 位整数装得下，
         * 不能先转 int32，否则 0x80000000 以上全变成负数 */
        lua_pushinteger(L, (lua_Integer)(uint32_t)v->i);
        break;
    case PBC_REAL:
        lua_pushnumber(L, v->f);
        break;
    case PBC_BOOL:
        lua_pushboolean(L, v->i != 0);
        break;
    case PBC_BYTES:
    case PBC_STRING:
        lua_pushlstring(L, v->s ? v->s : "", v->len);
        break;
    case PBC_FIXED64:
    case PBC_UINT:
    case PBC_INT64:
        // TODO 由于需要用位移，需要用uint64_t，由于lua不支持uint64_t，所以fixed64也是按int64_t来处理
        lua_pushinteger(L, (lua_Integer)v->i);
        break;
    default:
        std::fprintf(stderr, "pbc fast decode: unknown field type %d\n", type);
        lua_pushnil(L);
        break;
    }
}

// proto3 缺省字段的默认值。map entry 的 key 不能用 nil，否则无法 rawset
static inline void push_default(lua_State *L, const struct pbc_ffield *ff)
{
    switch (ff->type & ~PBC_REPEATED)
    {
    case PBC_REAL:
        lua_pushnumber(L, 0.0);
        break;
    case PBC_BOOL:
        lua_pushboolean(L, 0);
        break;
    case PBC_BYTES:
    case PBC_STRING:
        lua_pushlstring(L, "", 0);
        break;
    case PBC_MESSAGE:
        lua_createtable(L, 0, pbc_msg_field_count(ff->sub));
        break;
    default:
        lua_pushinteger(L, 0);
        break;
    }
}

/* MapEntry 里每解出一个字段回调一次；同一个字段重复出现时后写覆盖前写，
 * 与 protobuf map 的重复 key 语义一致。 */
static int map_entry_cb(void *ud, const struct pbc_ffield *field,
                        const struct pbc_fvalue *v)
{
    if (field->id != 1 && field->id != 2) return 0;

    MapEntryCtx *ctx = (MapEntryCtx *)ud;
    lua_State *L     = ctx->L;
    int32_t type     = field->type & ~PBC_REPEATED;
    if (type == PBC_MESSAGE)
    {
        struct pbc_slice sub;
        sub.buffer = const_cast<char *>(v->s);
        sub.len    = v->len;
        if (ctx->pc->decode_fast_message(L, field->sub, &sub) < 0) return -1;
    }
    else
    {
        push_scalar(L, type, v);
    }
    lua_replace(L, field->id == 1 ? ctx->key_idx : ctx->val_idx);
    return 0;
}

/* core 每解出一个值回调一次。这里直接用 lua_pushstring + lua_rawset，
 * 不再经过 lua_setfield（含 __newindex 检查与 luaH_get）；子 message 用
 * 预编译好的 _message* 指针递归，不再按 schema 字符串重查哈希表。 */
static int fast_decode_cb(void *ud, const struct pbc_ffield *field,
                          const struct pbc_fvalue *v)
{
    FastCtx *ctx = (FastCtx *)ud;
    lua_State *L = ctx->L;
    int32_t type = field->type;
    int32_t raw  = type & ~PBC_REPEATED;

    /* proto3 map：core 给的是一个个 MapEntry 子消息，这里直接还原成 kv table。
     * 不能复用 repeated 的 id/n 状态机，否则与其它 repeated 字段交错时会串表。 */
    if (field->is_map)
    {
        if (ctx->id)
        {
            ctx->id = 0;
            lua_rawset(L, -3); // 收掉上一个数组
        }
        lua_pushstring(L, field->name);
        lua_rawget(L, ctx->result);
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
            lua_createtable(L, 0, 0);
            lua_pushstring(L, field->name);
            lua_pushvalue(L, -2);
            lua_rawset(L, ctx->result);
        }
        struct pbc_slice sub;
        sub.buffer = const_cast<char *>(v->s);
        sub.len    = v->len;
        if (ctx->pc->decode_map_entry(L, field->sub, &sub) < 0) return -1;
        lua_rawset(L, -3); // kv[key] = value
        lua_pop(L, 1);     // 弹掉 kv，栈顶恢复为结果表
        return 0;
    }

    if (type & PBC_REPEATED)
    {
        if (field->id != ctx->id)
        {
            if (ctx->id)
            {
                lua_rawset(L, -3); // 收掉上一个数组
            }
            ctx->id = field->id;
            ctx->n  = 0;
            lua_pushstring(L, field->name);
            // packed 时 core 预算出了元素个数，一次分配到位，避免逐元素 rehash
            lua_createtable(L, v->count > 0 ? v->count : 0, 0);
        }
        if (raw == PBC_MESSAGE)
        {
            struct pbc_slice sub;
            sub.buffer = const_cast<char *>(v->s);
            sub.len    = v->len;
            if (ctx->pc->decode_fast_message(L, field->sub, &sub) < 0) return -1;
        }
        else
        {
            push_scalar(L, raw, v);
        }
        lua_rawseti(L, -2, ++ctx->n);
    }
    else
    {
        if (ctx->id)
        {
            ctx->id = 0;
            lua_rawset(L, -3); // 上一个数组结束了
        }
        lua_pushstring(L, field->name);
        if (raw == PBC_MESSAGE)
        {
            struct pbc_slice sub;
            sub.buffer = const_cast<char *>(v->s);
            sub.len    = v->len;
            if (ctx->pc->decode_fast_message(L, field->sub, &sub) < 0) return -1;
        }
        else
        {
            push_scalar(L, raw, v);
        }
        lua_rawset(L, -3);
    }
    return 0;
}

static void pbc_deleter(struct pbc_env *env)
{
    // 最后一个线程删除时，env最终失效。全局也必须重置，免得被新线程赋值
    // 虽然一般情况下最后一个线程失效时就只能是关服

    pbc_delete(env);
}

void PbcCodec::uninitialize()
{
    // 其实这里不释放没问题。g_env_在析构时会自动释放
    // 但由于它是static变量又没有放到static_global中，析构顺序无法保证会影响内存debug
    // 此时所有线程已停止，但读写仍统一走原子接口
    std::atomic_store_explicit(&g_env_,
        std::shared_ptr<struct pbc_env>(), std::memory_order_release);
}

PbcCodec::PbcCodec()
{
    write_msg_ = nullptr;
}

PbcCodec::~PbcCodec()
{
    if (write_msg_)
    {
        pbc_wmessage_delete(write_msg_);
        write_msg_ = nullptr;
    }
}

void PbcCodec::reset()
{
    // env 变了，缓存的 wmessage 里还持有旧 env 的 message 指针，必须丢弃
    if (write_msg_)
    {
        pbc_wmessage_delete(write_msg_);
        write_msg_ = nullptr;
    }
    // 只创建待加载的env，不发布。中途加载失败时旧的g_env_仍然可用
    load_env_ = std::shared_ptr<struct pbc_env>(pbc_new(), pbc_deleter);
}

void PbcCodec::load_finish()
{
    if (!load_env_)
    {
        ELOG("pbc load_finish: no env loading");
        return;
    }

    // 所有pb都注册完成后一次性发布，release与worker的acquire配对
    std::atomic_store_explicit(&g_env_, load_env_, std::memory_order_release);
    load_env_.reset();
}

void PbcCodec::update()
{
    if (write_msg_)
    {
        pbc_wmessage_delete(write_msg_);
        write_msg_ = nullptr;
    }
    env_ = std::atomic_load_explicit(&g_env_, std::memory_order_acquire);
}

// 按名字查一次 schema，再线性扫字段表；只在配置校验等冷路径调用
bool PbcCodec::is_map_field(const char *schema, const char *key)
{
    if (!env_)
    {
        ELOG("pbc is_map: env not initialized");
        return false;
    }
    struct pbc_msg *msg = pbc_msg_from_name(env_.get(), schema);
    if (!msg)
    {
        ELOG("pbc is_map: no such message %s", schema);
        return false;
    }
    int32_t n = pbc_msg_field_cap(msg);
    for (int32_t i = 0; i < n; i++)
    {
        const struct pbc_ffield *ff = pbc_msg_field_at(msg, i);
        if (ff && strcmp(ff->name, key) == 0)
        {
            return ff->is_map != 0;
        }
    }
    ELOG("pbc is_map: field(%s) not found in %s", key, schema);
    return false;
}

int32_t PbcCodec::is_map(lua_State *L)
{
    const char *schema = luaL_checkstring(L, 2);
    const char *key    = luaL_checkstring(L, 3);

    lua_pushboolean(L, is_map_field(schema, key));
    return 1;
}

const char *PbcCodec::last_error()
{
    const char *env_e = pbc_error(env_.get());
    if (env_e && strlen(env_e) > 0)
    {
        error_msg_.append("\n").append(env_e);
    }
    error_msg_.append(" @ ");

    std::vector<std::string>::reverse_iterator rit = trace_back_.rbegin();
    while (rit != trace_back_.rend())
    {

        error_msg_.append("[").append(*rit).append("]");

        rit++;
    }

    return error_msg_.c_str();
}

int32_t PbcCodec::load(lua_State *L)
{
    size_t size = 0;
    struct pbc_slice slice;

    slice.buffer = (void *)luaL_checklstring(L, 2, &size);
    slice.len    = (int32_t)size;

    if (!load_env_)
    {
        ELOG("pbc register: no env loading, call reset first");
        return 0;
    }

    if (0 != pbc_register(load_env_.get(), &slice))
    {
        ELOG("pbc register error:%s", pbc_error(load_env_.get()));
        return 0;
    }

    lua_pushboolean(L, 1);
    return 1;
}

void PbcCodec::clear_last()
{
    // write_msg_ 跨调用复用，只在 raw_encode 里 reset，不再每次 delete
    error_msg_.clear();
    trace_back_.clear();
}

int32_t PbcCodec::decode_fast_message(lua_State *L, struct pbc_msg *msg,
                                      struct pbc_slice *slice)
{
    if (!msg)
    {
        // 保持调用点的栈平衡：回调约定本函数一定在栈顶留下一个值
        error_msg_ = "protobuf decode: unresolved sub message";
        lua_pushnil(L);
        return -1;
    }
    if (!lua_checkstack(L, 4))
    {
        error_msg_ = "protobuf decode stack overflow";
        return -1;
    }

    // 按 schema 字段数一次分配到位，避免从 0 槽反复 rehash + insertkey
    lua_createtable(L, 0, pbc_msg_field_count(msg));

    // 每层 message 一个 ctx，只能用栈变量，不能放成员变量
    FastCtx ctx;
    ctx.L      = L;
    ctx.result = lua_gettop(L);
    ctx.id     = 0;
    ctx.n      = 0;
    ctx.pc     = this;

    int32_t ok = pbc_decode_fast_msg(msg, slice, fast_decode_cb, &ctx);
    // 结束上一个数组
    if (ctx.id)
    {
        lua_rawset(L, -3);
    }
    if (ok < 0)
    {
        error_msg_ = "protobuf decode: invalid wire data";
        lua_pop(L, 1);
        return -1;
    }
    return 1;
}

/* 解开一个 MapEntry 子消息，在栈顶留下 key、value 两个值。
 * 缺省字段先按 proto3 默认值压栈：pbc 不填充默认值，而 map 的 key 为 nil
 * 会让 lua_rawset 直接报错，value 为 nil 又会丢掉整个键。 */
int32_t PbcCodec::decode_map_entry(lua_State *L, struct pbc_msg *entry,
                                   struct pbc_slice *slice)
{
    if (!entry)
    {
        ELOG("pbc map decode: unresolved map entry message");
        error_msg_ = "protobuf decode: unresolved map entry message";
        return -1;
    }
    const struct pbc_ffield *kff = pbc_msg_field_at(entry, 0);
    const struct pbc_ffield *vff = pbc_msg_field_at(entry, 1);
    if (!kff || !vff || kff->id != 1 || vff->id != 2 ||
        strcmp(kff->name, "key") != 0 || strcmp(vff->name, "value") != 0)
    {
        ELOG("pbc map decode: broken map entry schema");
        error_msg_ = "protobuf decode: broken map entry schema";
        return -1;
    }
    if (!lua_checkstack(L, 3))
    {
        ELOG("pbc map decode: lua stack overflow");
        error_msg_ = "protobuf decode: map entry stack overflow";
        return -1;
    }

    push_default(L, kff);
    push_default(L, vff);
    MapEntryCtx ctx;
    ctx.L       = L;
    ctx.pc      = this;
    ctx.val_idx = lua_gettop(L);
    ctx.key_idx = ctx.val_idx - 1;

    if (pbc_decode_fast_msg(entry, slice, map_entry_cb, &ctx) < 0)
    {
        ELOG("pbc map decode: invalid map entry wire data");
        error_msg_ = "protobuf decode: invalid map entry wire data";
        lua_pop(L, 2);
        return -1;
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * 编码：遍历 schema，回取 table
 *
 * 字段表由 seal 阶段预编译（pbc_msg_field_at，按 field id 升序），编码时正向
 * 走一遍：push 字段名 -> rawget 取业务对象上的值 -> 为 nil 就跳过。
 *
 * 与"遍历 table 反查 schema"相比：
 *   + 业务对象上的多余键完全不参与（直接发实体表时不再为每个多余键付一次
 *     哈希未命中）
 *   + 字段顺序固定；子 message 指针随元数据直达，不按 schema 名重查哈希
 *   - 业务对象缺字段也要付一次 rawget + isnil（宽表稀疏填充会慢一些）
 * 必须用 lua_rawget：lua_getfield 会触发业务对象上的 __index，语义不同。
 * ------------------------------------------------------------------------- */
int32_t PbcCodec::encode_message(lua_State *L, struct pbc_wmessage *wmsg,
                                 struct pbc_msg *msg, int32_t index)
{
    if (!lua_istable(L, index))
    {
        error_msg_ = STD_FMT("protobuf encode expect a table at %d", index);
        return -1;
    }

    if (!lua_checkstack(L, 4))
    {
        error_msg_ = STD_FMT("protobuf encode stack overflow: %d", lua_gettop(L));
        return -1;
    }

    int32_t top = lua_gettop(L);
    int32_t n   = pbc_msg_field_cap(msg);

    for (int32_t i = 0; i < n; i++)
    {
        const struct pbc_ffield *ff = pbc_msg_field_at(msg, i);
        if (!ff)
        {
            // decode_cap 是连续写入的，取不到 NULL；真出现说明缓存被破坏
            error_msg_ = STD_FMT("protobuf encode: broken field cache of %s",
                                 pbc_msg_key(msg));
            lua_settop(L, top);
            return -1;
        }

        lua_pushstring(L, ff->name);
        lua_rawget(L, index);
        if (lua_isnil(L, -1))
        {
            // 业务对象可以不带这个字段（proto3 的默认值语义）
            lua_pop(L, 1);
            continue;
        }

        if (encode_field(L, wmsg, ff, lua_gettop(L)) < 0)
        {
            trace_back_.push_back(pbc_msg_key(msg));
            lua_settop(L, top);
            return -1;
        }
        lua_pop(L, 1);
    }

    lua_settop(L, top);
    return 0;
}

int32_t PbcCodec::encode_field(lua_State *L, struct pbc_wmessage *wmsg,
                               const struct pbc_ffield *ff, int32_t index)
{
    /* 字段句柄与元数据在同一块记录里，这一步只是把只读元数据转成 core 的
     * 不透明句柄，不做任何查找。 */
    struct pbc_wfield *field = pbc_wmessage_field_at(wmsg, ff);
    if (!field)
    {
        trace_back_.push_back(ff->name);
        error_msg_ = STD_FMT("field(%s) no field handle", ff->name);
        return -1;
    }

    // proto3 map 优先分派：它也是 repeated message，但不能走数组语义
    if (ff->is_map)
    {
        return encode_map(L, wmsg, field, ff, index);
    }

    if ((ff->type & ~PBC_REPEATED) == PBC_MESSAGE)
    {
        return encode_sub(L, wmsg, field, ff, index);
    }

    // 单值标量是最常见的形态，直接进类型分派，不额外过一层 repeated 判断
    if (!(ff->type & PBC_REPEATED))
    {
        return encode_scalar(L, wmsg, field, ff->type, index, ff->name);
    }
    return encode_scalar_list(L, wmsg, field, ff->type, index, ff->name);
}

int32_t PbcCodec::encode_sub(lua_State *L, struct pbc_wmessage *wmsg,
                             struct pbc_wfield *field,
                             const struct pbc_ffield *ff, int32_t index)
{
    if (!ff->sub)
    {
        // 跨文件引用没解析出来时 seal 也拿不到子 message 指针
        trace_back_.push_back(ff->name);
        error_msg_ = STD_FMT("field(%s) unresolved sub message", ff->name);
        return -1;
    }

    int32_t top   = lua_gettop(L);
    bool repeated = (ff->type & PBC_REPEATED) != 0;

    if (repeated)
    {
        if (!lua_istable(L, index))
        {
            trace_back_.push_back(ff->name);
            error_msg_ = STD_FMT("field(%s) expect a table,got %s", ff->name,
                                 lua_typename(L, lua_type(L, index)));
            return -1;
        }
        lua_pushnil(L);
    }

    for (;;)
    {
        // 非 repeated 只跑一次；repeated 由 lua_next 驱动
        if (repeated && !lua_next(L, index))
        {
            return 0;
        }

        int32_t vi = lua_gettop(L);
        if (!lua_istable(L, vi))
        {
            trace_back_.push_back(ff->name);
            error_msg_ = STD_FMT("field(%s) expect table,got %s", ff->name,
                                 lua_typename(L, lua_type(L, vi)));
            lua_settop(L, top);
            return -1;
        }

        struct pbc_wmessage *submsg = pbc_wmessage_message_f(wmsg, field);
        if (!submsg)
        {
            trace_back_.push_back(ff->name);
            error_msg_ = STD_FMT("field(%s) create sub message failed", ff->name);
            lua_settop(L, top);
            return -1;
        }

        // 子层的值就在栈顶（非 repeated 时恰好是 index，也是栈顶）
        int32_t ret = encode_message(L, submsg, ff->sub, vi);
        pbc_wmessage_end_sub(submsg);
        if (ret < 0)
        {
            lua_settop(L, top);
            return -1;
        }

        if (!repeated)
        {
            return 0;
        }
        lua_pop(L, 1); // 弹掉 value，key 留给下一轮 lua_next
    }
}

/* proto3 map：Lua 侧直接给 kv table，每个键值对编码成一个 MapEntry 子消息。
 * MapEntry 的 key=1 / value=2 字段句柄来自 seal 阶段预编译的字段表，
 * 不需要按字段名再查一次哈希。 */
int32_t PbcCodec::encode_map(lua_State *L, struct pbc_wmessage *wmsg,
                             struct pbc_wfield *field,
                             const struct pbc_ffield *ff, int32_t index)
{
    if (!ff->sub)
    {
        trace_back_.push_back(ff->name);
        error_msg_ = STD_FMT("field(%s) unresolved map entry", ff->name);
        ELOG("pbc map encode: %s", error_msg_.c_str());
        return -1;
    }
    const struct pbc_ffield *kff = pbc_msg_field_at(ff->sub, 0);
    const struct pbc_ffield *vff = pbc_msg_field_at(ff->sub, 1);
    if (!kff || !vff || kff->id != 1 || vff->id != 2 ||
        strcmp(kff->name, "key") != 0 || strcmp(vff->name, "value") != 0)
    {
        trace_back_.push_back(ff->name);
        error_msg_ = STD_FMT("field(%s) broken map entry schema", ff->name);
        ELOG("pbc map encode: %s", error_msg_.c_str());
        return -1;
    }
    if (!lua_istable(L, index))
    {
        trace_back_.push_back(ff->name);
        error_msg_ = STD_FMT("field(%s) expect a table,got %s", ff->name,
                             lua_typename(L, lua_type(L, index)));
        ELOG("pbc map encode: %s", error_msg_.c_str());
        return -1;
    }

    int32_t top = lua_gettop(L);
    lua_pushnil(L);
    while (lua_next(L, index))
    {
        int32_t vi = lua_gettop(L); // value
        int32_t ki = vi - 1;        // key

        // bool 键必须用 Lua boolean：整数 0/1 在 Lua 里都是 truthy，会写错
        if ((kff->type & ~PBC_REPEATED) == PBC_BOOL && !lua_isboolean(L, ki))
        {
            trace_back_.push_back(ff->name);
            error_msg_ = STD_FMT("field(%s) expect boolean map key,got %s",
                                 ff->name, lua_typename(L, lua_type(L, ki)));
            ELOG("pbc map encode: %s", error_msg_.c_str());
            lua_settop(L, top);
            return -1;
        }

        struct pbc_wmessage *entry = pbc_wmessage_message_f(wmsg, field);
        if (!entry)
        {
            trace_back_.push_back(ff->name);
            error_msg_ = STD_FMT("field(%s) create map entry failed", ff->name);
            ELOG("pbc map encode: %s", error_msg_.c_str());
            lua_settop(L, top);
            return -1;
        }

        int32_t ret = 0;
        struct pbc_wfield *kf = pbc_wmessage_field_at(entry, kff);
        if (!kf || encode_scalar(L, entry, kf, kff->type, ki, "key") < 0)
        {
            if (error_msg_.empty())
            {
                error_msg_ = STD_FMT("field(%s) write map key failed", ff->name);
            }
            ret = -1;
        }

        if (ret == 0)
        {
            struct pbc_wfield *vf = pbc_wmessage_field_at(entry, vff);
            if (!vf)
            {
                error_msg_ = STD_FMT("field(%s) no value field handle", ff->name);
                ret = -1;
            }
            else if ((vff->type & ~PBC_REPEATED) == PBC_MESSAGE)
            {
                // map value 是 message：再开一层子视图递归编码
                struct pbc_wmessage *sub = pbc_wmessage_message_f(entry, vf);
                if (!sub)
                {
                    error_msg_ = STD_FMT("field(%s) create map value failed", ff->name);
                    ret = -1;
                }
                else
                {
                    ret = encode_message(L, sub, vff->sub, vi);
                    pbc_wmessage_end_sub(sub);
                }
            }
            else
            {
                ret = encode_scalar(L, entry, vf, vff->type, vi, "value");
            }
        }
        pbc_wmessage_end_sub(entry);
        if (ret < 0)
        {
            if (error_msg_.empty())
            {
                error_msg_ = STD_FMT("field(%s) write map value failed", ff->name);
            }
            ELOG("pbc map encode: %s", error_msg_.c_str());
            lua_settop(L, top);
            return -1;
        }
        lua_pop(L, 1); // 弹掉 value，key 留给下一轮 lua_next
    }
    lua_settop(L, top);
    return 0;
}

int32_t PbcCodec::encode_scalar_list(lua_State *L, struct pbc_wmessage *wmsg,
                                     struct pbc_wfield *field, int32_t type,
                                     int32_t index, const char *key)
{
    // 只处理 repeated：单值分支在 encode_field 里已经直接分派掉了
    if (!lua_istable(L, index))
    {
        trace_back_.push_back(key);
        error_msg_ = STD_FMT("field(%s) expect a table,got %s", key,
                             lua_typename(L, lua_type(L, index)));
        return -1;
    }

    int32_t raw_type = type & ~PBC_REPEATED;
    int32_t top      = lua_gettop(L);

    /* packed 数组批量写入：一次 begin，循环里只做一次取值 + 一次 C 调用，
     * 不再走 encode_scalar 的完整类型分派与默认值判断。payload 直接追加到
     * 父 buffer，不再攒 pbc_array 后第二趟重编码。 */
    if (pbc_wmessage_packed_begin(wmsg, field))
    {
        bool failed = false;
        lua_pushnil(L);
        while (lua_next(L, index))
        {
            int32_t vi = lua_gettop(L);
            switch (raw_type)
            {
            case PBC_REAL:
                if (!lua_isnumber(L, vi))
                {
                    trace_back_.push_back(key);
                    error_msg_ = STD_FMT("field(%s) expect number,got %s", key,
                                         lua_typename(L, lua_type(L, vi)));
                    failed = true;
                    break;
                }
                pbc_wmessage_packed_real(wmsg, field, lua_tonumber(L, vi));
                break;
            case PBC_BOOL:
                pbc_wmessage_packed_integer(
                    wmsg, field, (uint32_t)lua_toboolean(L, vi), 0);
                break;
            case PBC_INT:
            case PBC_UINT32:
            case PBC_SFIXED32:
            case PBC_FIXED32:
            case PBC_UINT:
            case PBC_FIXED64:
            case PBC_INT64:
            case PBC_ENUM:
            {
                if (!lua_isinteger(L, vi))
                {
                    trace_back_.push_back(key);
                    error_msg_ = STD_FMT("field(%s) expect integer,got %s", key,
                                         lua_typename(L, lua_type(L, vi)));
                    failed = true;
                    break;
                }
                int64_t val = (int64_t)lua_tointeger(L, vi);
                pbc_wmessage_packed_integer(
                    wmsg, field, (uint32_t)val, (uint32_t)(val >> 32));
                break;
            }
            default:
                trace_back_.push_back(key);
                error_msg_ = STD_FMT("field(%s) unsupported packed type %d",
                                     key, raw_type);
                failed = true;
                break;
            }
            lua_pop(L, 1);
            if (failed) break;
        }
        lua_settop(L, top);
        pbc_wmessage_packed_end(wmsg);
        return failed ? -1 : 0;
    }

    lua_pushnil(L);
    while (lua_next(L, index))
    {
        if (encode_scalar(L, wmsg, field, raw_type, lua_gettop(L), key) < 0)
        {
            lua_settop(L, top);
            return -1;
        }
        lua_pop(L, 1);
    }

    return 0;
}

int32_t PbcCodec::encode_scalar(lua_State *L, struct pbc_wmessage *wmsg,
                                struct pbc_wfield *field, int32_t type,
                                int32_t index, const char *key)
{
#define LUAL_CHECK(TYPE)                                               \
    if (!lua_is##TYPE(L, index))                                       \
    {                                                                  \
        trace_back_.push_back(key);                                    \
        error_msg_ = STD_FMT("field(%s) expect " #TYPE ",got %s", key, \
                             lua_typename(L, lua_type(L, index)));     \
        return -1;                                                     \
    }

    switch (type)
    {
    case PBC_INT:
    case PBC_UINT32:
    case PBC_SFIXED32:
    case PBC_FIXED32:
    case PBC_UINT:
    case PBC_FIXED64:
    case PBC_INT64:
    {
        LUAL_CHECK(integer)
        int64_t val = (int64_t)lua_tointeger(L, index);
        uint32_t hi = (uint32_t)(val >> 32);
        pbc_wmessage_integer_f(wmsg, field, (uint32_t)val, hi);
    }
    break;
    case PBC_REAL:
    {
        LUAL_CHECK(number)
        double val = lua_tonumber(L, index);
        pbc_wmessage_real_f(wmsg, field, val);
    }
    break;
    case PBC_BOOL:
    {
        int32_t val = lua_toboolean(L, index);
        pbc_wmessage_integer_f(wmsg, field, (uint32_t)val, 0);
    }
    break;
    case PBC_ENUM:
    {
        LUAL_CHECK(integer)
        int32_t val = (int32_t)lua_tointeger(L, index);
        pbc_wmessage_integer_f(wmsg, field, (uint32_t)val, 0);
    }
    break;
    case PBC_STRING:
    case PBC_BYTES:
    {
        LUAL_CHECK(string)
        size_t len      = 0;
        const char *val = lua_tolstring(L, index, &len);
        if (pbc_wmessage_string_f(wmsg, field, val, (int32_t)len))
        {
            error_msg_ = STD_FMT("field(%s) write string error", key);
            return -1;
        }
    }
    break;
    default: error_msg_ = STD_FMT("protobuf unknow type: %d", type); return -1;
    }
    return 0;

#undef LUAL_CHECK
}

int32_t PbcCodec::raw_decode(lua_State *L, const char *schema,
                             const char *buffer, size_t size)
{
    assert(env_);
    clear_last();

    struct pbc_slice slice;
    slice.len    = static_cast<int32_t>(size);
    slice.buffer = const_cast<char *>(buffer);

    /* 顶层只按名字查一次 message，之后所有子层都用 seal 阶段缓存的
     * struct _message* 指针递归，不再重复做字符串哈希。 */
    struct pbc_msg *msg = pbc_msg_from_name(env_.get(), schema);
    if (!msg)
    {
        error_msg_ = "protobuf decode: no such message: ";
        error_msg_ += schema;
        return luaL_error(L, "%s", last_error());
    }

    if (decode_fast_message(L, msg, &slice) < 0)
    {
        return luaL_error(L, "%s", last_error());
    }

    // 默认情况下，所有内容解析到一个table
    return 1;
}

int32_t PbcCodec::decode(lua_State *L)
{
    const char *schema = luaL_checkstring(L, 2);

    size_t size        = 0;
    const char *buffer = luaL_checklstring(L, 3, &size);

    return raw_decode(L, schema, buffer, size);
}

int32_t PbcCodec::raw_encode(lua_State *L, const char *schema, int32_t index)
{
    assert(env_);
    clear_last();

    /* 顶层按名字查一次 message：之后字段遍历、子 message 递归都直接用指针，
     * reset 也走指针版本，不再第二次按名字哈希。 */
    struct pbc_msg *msg = pbc_msg_from_name(env_.get(), schema);
    if (!msg)
    {
        error_msg_ = STD_FMT("no such protobuf message found: %s", schema);
        return -1;
    }

    /* 复用上一次的 wmessage：只重置内容、保留 heap，省掉每次编码的
     * malloc/free（小包场景下固定开销占比很高）。 */
    if (write_msg_)
    {
        write_msg_ = pbc_wmessage_reset_msg(write_msg_, msg);
    }
    if (!write_msg_)
    {
        write_msg_ = pbc_wmessage_new(env_.get(), schema);
        if (!write_msg_)
        {
            error_msg_ = STD_FMT("create wmessage failed: %s", schema);
            return -1;
        }
    }

    if (encode_message(L, write_msg_, msg, index) < 0)
    {
        return luaL_error(L, "%s", last_error());
    }

    return 1;
}

int32_t PbcCodec::encode(lua_State *L)
{
    const char *schema = luaL_checkstring(L, 2);

    if (raw_encode(L, schema, 3) < 0)
    {
        return luaL_error(L, "%s", last_error());
    }

    struct pbc_slice slice;
    pbc_wmessage_buffer(write_msg_, &slice);

    lua_pushlstring(L, (const char *)slice.buffer, slice.len);
    return 1;
}

int32_t PbcCodec::decode_from_buffer(lua_State *L)
{
    const char *schema = luaL_checkstring(L, 2);
    const char *buffer = (const char *)lua_touserdata(L, 3);
    lua_Integer size   = luaL_checkinteger(L, 4);

    // 入口必须校验：userdata 之外的参数会让 buffer 为 NULL、size 无法解释
    if (!buffer || size < 0)
    {
        return luaL_error(L, "decode_from_buffer: invalid buffer(%p) or size(%d)",
                          (const void *)buffer, (int)size);
    }

    return raw_decode(L, schema, buffer, (size_t)size);
}

int32_t PbcCodec::encode_to_buffer(lua_State *L)
{
    const char *schema = luaL_checkstring(L, 2);

    if (raw_encode(L, schema, 3) < 0)
    {
        return luaL_error(L, "%s", last_error());
    }

    struct pbc_slice slice;
    pbc_wmessage_buffer(write_msg_, &slice);

    // slice只是引用指针，数据是存write_msg_上的，因此可以安全push到lua
    lua_pushlightuserdata(L, slice.buffer);
    lua_pushinteger(L, slice.len);

    return 2;
}