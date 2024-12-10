#include <cstdarg>
#include <chrono>

#include "lbson.hpp"

#include "ltools.hpp"
#include "lmongo.hpp"
#include "system/static_global.hpp"

#define MONGODB_EVENT "mongodb_event"

LMongo::LMongo(lua_State *L)
    : Thread("lmongo"), query_pool_("lmongo"), result_pool_("lmongo")
{
    array_opt_ = -1; // 是否启用数组判定参数，具体参考lbson的check_type函数
    dbid_ = luaL_checkinteger32(L, 2);

    if (lua_isstring(L, 3))
    {
        const char *name = lua_tostring(L, 3);
        set_thread_name(name);
    }
    else
    {
        ELOG("invalid mongodb thread name");
    }
}

LMongo::~LMongo()
{
    if (!query_.empty())
    {
        ELOG("mongo query not clean, abort");
        while (!query_.empty())
        {
            query_pool_.destroy(query_.front());
            query_.pop();
        }
    }

    if (!result_.empty())
    {
        ELOG("mongo result not clean, abort");
        while (!result_.empty())
        {
            result_pool_.destroy(result_.front());
            result_.pop();
        }
    }
}

// 连接数据库
// 由于是开启线程去连接，并且是异步的，需要上层定时调用valid来检测是否连接上
int32_t LMongo::start(lua_State *L)
{
    if (active())
    {
        return luaL_error(L, "mongo thread already active");
    }

    const char *ip     = luaL_checkstring(L, 1);
    const int32_t port = luaL_checkinteger32(L, 2);
    const char *usr    = luaL_checkstring(L, 3);
    const char *pwd    = luaL_checkstring(L, 4);
    const char *db     = luaL_checkstring(L, 5);

    set(ip, port, usr, pwd, db);
    Thread::start(5000);

    return 0;
}

int32_t LMongo::stop(lua_State *L)
{
    UNUSED(L);
    Thread::stop();

    return 0;
}

bool LMongo::initialize()
{
    if (connect())
    {
        ELOG("mongo connect fail");
        return false;
    }

    // 不断地重试，直到连上重试
    int32_t ok = 0;
    do
    {
        ok = ping();

        // 连接成功
        if (0 == ok) break;

        // 连接出错，直接退出
        if (ok > 0)
        {
            disconnect();
            return false;
        }

        // 连接进行中，继续阻塞等待
        std::this_thread::sleep_for(std::chrono::seconds(1));
    } while (active());

    if (0 == ok) wakeup_main(S_READY);

    return true;
}

void LMongo::routine_once(int32_t ev)
{
    UNUSED(ev);
    /* 如果某段时间连不上，只能由下次超时后触发
     * 超时时间由thread::start参数设定
     */
    if (ping()) return;

    std::unique_lock<std::mutex> ul(mutex_);

    while (!query_.empty())
    {
        MongoQuery *query = query_.front();
        query_.pop();

        MongoResult *res = result_pool_.construct(query->qid_, query->mqt_);

        ul.unlock();
        bool ok = do_command(query, res);
        ul.lock();

        query_pool_.destroy(query);
        if (ok)
        {
            result_.push(res);
            wakeup_main(S_DATA);
        }
        else
        {
            ELOG("mongodb command fail, qid = %d !!!", res->qid_);
            result_pool_.destroy(res);
        }
    }
}

bool LMongo::uninitialize()
{
    disconnect();

    return true;
}

void LMongo::on_ready(lua_State *L)
{
    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, MONGODB_EVENT);
    lua_pushinteger(L, S_READY);
    lua_pushinteger(L, dbid_);

    if (LUA_OK != lua_pcall(L, 2, 0, 1))
    {
        ELOG("mongodb on ready error:%s", lua_tostring(L, -1));
        lua_pop(L, 1); /* remove error message */
    }

    lua_pop(L, 1);
}

void LMongo::main_routine(int32_t ev)
{
    lua_State *L = StaticGlobal::L;

    if (unlikely(ev & S_READY)) on_ready(L);

    LUA_PUSHTRACEBACK(L);

    std::unique_lock<std::mutex> ul(mutex_);

    while (!result_.empty())
    {
        MongoResult *res = result_.front();
        result_.pop();

        ul.unlock();
        on_result(L, res);
        ul.lock();

        result_pool_.destroy(res);
    }

    lua_pop(L, 1); /* remove stacktrace */
}

void LMongo::on_result(lua_State *L, const MongoResult *res)
{
    // 打印错误日志，有时候脚本不设置回调函数，不打印出错就没法知道了
    // TODO 要不要把错误信息存起来，做个last_e函数提供给脚本获取错误信息
    if (0 != res->error_.code)
    {
        ELOG("ERROR: qid = %d, e = %d, %s", res->qid_, res->error_.code,
             res->error_.message);
    }

    // 为0表示不需要回调到脚本
    if (0 == res->qid_) return;

    lua_getglobal(L, MONGODB_EVENT);

    lua_pushinteger(L, S_DATA);
    lua_pushinteger(L, dbid_);
    lua_pushinteger(L, res->qid_);
    lua_pushinteger(L, res->error_.code);

    int32_t nargs = 4;
    if (res->data_)
    {
        bson_error_t e;
        bson_type_t type =
            res->mqt_ == MQT_FIND ? BSON_TYPE_ARRAY : BSON_TYPE_DOCUMENT;

        if (decode(L, res->data_, &e, type, array_opt_) < 0)
        {
            lua_pop(L, 4);
            ELOG("mongo result decode error:%s", e.message);

            // 即使出错，也回调到脚本
        }
        else
        {
            nargs++;
        }
    }

    if (LUA_OK != lua_pcall(L, nargs, 0, 1))
    {
        ELOG("mongo call back error:%s", lua_tostring(L, -1));
        lua_pop(L, 1); /* remove error message */
    }
}

/* 在子线程触发查询命令 */
bool LMongo::do_command(const MongoQuery *query, MongoResult *res)
{
    bool ok    = false;
    auto begin = std::chrono::steady_clock::now();
    switch (query->mqt_)
    {
    case MQT_COUNT: ok = Mongo::count(query, res); break;
    case MQT_FIND: ok = Mongo::find(query, res); break;
    case MQT_FMOD: ok = Mongo::find_and_modify(query, res); break;
    case MQT_INSERT: ok = Mongo::insert(query, res); break;
    case MQT_UPDATE: ok = Mongo::update(query, res); break;
    case MQT_REMOVE: ok = Mongo::remove(query, res); break;
    default:
    {
        ELOG("unknow handle mongo command type:%d\n", query->mqt_);
        return false;
    }
    }

    assert((ok && 0 == res->error_.code) || (!ok && 0 != res->error_.code));

    auto end = std::chrono::steady_clock::now();
    res->elaspe_ =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();

    return true;
}

int32_t LMongo::count(lua_State *L)
{
    if (!active())
    {
        return luaL_error(L, "mongo thread not active");
    }

    int32_t id             = luaL_checkinteger32(L, 1);
    const char *collection = luaL_checkstring(L, 2);
    if (!collection)
    {
        return luaL_error(L, "mongo count:collection not specify");
    }

    bson_t *query = bson_new_from_lua(L, 3, 0, array_opt_);
    bson_t *opts  = bson_new_from_lua(L, 4, 0, array_opt_, query);

    {
        std::lock_guard<std::mutex> guard(mutex_);

        MongoQuery *mongo_count =
            query_pool_.construct(id, MQT_COUNT, collection, query, opts);

        query_.push(mongo_count);
        wakeup(S_DATA);
    }

    return 0;
}

int32_t LMongo::find(lua_State *L)
{
    if (!active())
    {
        return luaL_error(L, "mongo thread not active");
    }

    int32_t id             = luaL_checkinteger32(L, 1);
    const char *collection = luaL_checkstring(L, 2);
    if (!collection)
    {
        return luaL_error(L, "mongo find:collection not specify");
    }

    bson_t *query = bson_new_from_lua(L, 3, 1, array_opt_);
    bson_t *opts  = bson_new_from_lua(L, 4, 0, array_opt_, query);

    {
        std::lock_guard<std::mutex> guard(mutex_);

        MongoQuery *mongo_find =
            query_pool_.construct(id, MQT_FIND, collection, query, opts);

        query_.push(mongo_find);
        wakeup(S_DATA);
    }

    return 0;
}

int32_t LMongo::find_and_modify(lua_State *L)
{
    if (!active())
    {
        return luaL_error(L, "mongo thread not active");
    }

    int32_t id             = luaL_checkinteger32(L, 1);
    const char *collection = luaL_checkstring(L, 2);
    if (!collection)
    {
        return luaL_error(L, "mongo find_and_modify:collection not specify");
    }

    bson_t *query  = bson_new_from_lua(L, 3, 1, array_opt_);
    bson_t *sort   = bson_new_from_lua(L, 4, 0, array_opt_, query);
    bson_t *update = bson_new_from_lua(L, 5, 1, array_opt_, query, sort);
    bson_t *fields = bson_new_from_lua(L, 6, 0, array_opt_, query, sort, update);

    bool remove  = lua_toboolean(L, 7);
    bool upsert  = lua_toboolean(L, 8);
    bool ret_new = lua_toboolean(L, 9);

    {
        std::lock_guard<std::mutex> guard(mutex_);

        MongoQuery *mongo_fmod =
            query_pool_.construct(id, MQT_FMOD, collection, query);

        mongo_fmod->sort_   = sort;
        mongo_fmod->update_ = update;
        mongo_fmod->fields_ = fields;

        mongo_fmod->remove_ = remove;
        mongo_fmod->upsert_ = upsert;
        mongo_fmod->new_    = ret_new;

        query_.push(mongo_fmod);
        wakeup(S_DATA);
    }

    return 0;
}

int32_t LMongo::set_array_opt(lua_State *L)
{
    array_opt_ = luaL_checknumber(L, 1);
    return 0;
}

/* insert( id,collections,info ) */
int32_t LMongo::insert(lua_State *L)
{
    if (!active())
    {
        return luaL_error(L, "mongo thread not active");
    }

    int32_t id             = luaL_checkinteger32(L, 1);
    const char *collection = luaL_checkstring(L, 2);
    if (!collection)
    {
        return luaL_error(L, "mongo insert:collection not specify");
    }

    bson_t *query = bson_new_from_lua(L, 3, -1, array_opt_);

    {
        std::lock_guard<std::mutex> guard(mutex_);

        MongoQuery *mongo_insert =
            query_pool_.construct(id, MQT_INSERT, collection, query);

        query_.push(mongo_insert);
        wakeup(S_DATA);
    }

    return 0;
}

int32_t LMongo::update(lua_State *L)
{
    if (!active())
    {
        return luaL_error(L, "mongo thread not active");
    }

    int32_t id             = luaL_checkinteger32(L, 1);
    const char *collection = luaL_checkstring(L, 2);
    if (!collection)
    {
        return luaL_error(L, "mongo update:collection not specify");
    }

    bson_t *query  = bson_new_from_lua(L, 3, -1, array_opt_);
    bson_t *update = bson_new_from_lua(L, 4, -1, array_opt_, query);

    int32_t upsert = lua_toboolean(L, 5);
    int32_t multi  = lua_toboolean(L, 6);

    {
        std::lock_guard<std::mutex> guard(mutex_);

        MongoQuery *mongo_update =
            query_pool_.construct(id, MQT_UPDATE, collection, query);
        mongo_update->update_ = update;
        mongo_update->flags_ =
            (upsert ? MONGOC_UPDATE_UPSERT : MONGOC_UPDATE_NONE)
            | (multi ? MONGOC_UPDATE_MULTI_UPDATE : MONGOC_UPDATE_NONE);

        query_.push(mongo_update);
        wakeup(S_DATA);
    }

    return 0;
}

int32_t LMongo::remove(lua_State *L)
{
    if (!active())
    {
        return luaL_error(L, "mongo thread not active");
    }

    int32_t id             = luaL_checkinteger32(L, 1);
    const char *collection = luaL_checkstring(L, 2);
    if (!collection)
    {
        return luaL_error(L, "mongo remove:collection not specify");
    }

    bson_t *query = bson_new_from_lua(L, 3, -1, array_opt_);

    int32_t single = lua_toboolean(L, 4);

    {
        std::lock_guard<std::mutex> guard(mutex_);

        MongoQuery *mongo_remove =
            query_pool_.construct(id, MQT_REMOVE, collection, query);
        mongo_remove->flags_ =
            single ? MONGOC_REMOVE_SINGLE_REMOVE : MONGOC_REMOVE_NONE;

        query_.push(mongo_remove);
        wakeup(S_DATA);
    }

    return 0;
}
