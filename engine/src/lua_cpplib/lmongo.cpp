#include <cstdarg>
#include <chrono>

#include <lbson.h>

#include "ltools.hpp"
#include "lmongo.hpp"
#include "../system/static_global.hpp"

#define MONGODB_EVENT "mongodb_event"

LMongo::LMongo(lua_State *L) : Thread("lmongo")
{
    _dbid = luaL_checkinteger(L, 2);
}

LMongo::~LMongo() {}

// 连接数据库
// 由于是开启线程去连接，并且是异步的，需要上层定时调用valid来检测是否连接上
int32_t LMongo::start(lua_State *L)
{
    if (active())
    {
        return luaL_error(L, "mongo thread already active");
    }

    const char *ip     = luaL_checkstring(L, 1);
    const int32_t port = luaL_checkinteger(L, 2);
    const char *usr    = luaL_checkstring(L, 3);
    const char *pwd    = luaL_checkstring(L, 4);
    const char *db     = luaL_checkstring(L, 5);

    set(ip, port, usr, pwd, db);
    Thread::start(5000000);

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
        ERROR("mongo connect fail");
        return false;
    }

    // 不断地重试，直到连上重试
    int32_t ok = 0;
    do
    {
        ok = ping();
        if (0 == ok) break;
        if (ok > 0) return false;

        std::this_thread::sleep_for(std::chrono::seconds(1));
    } while (active());

    if (0 == ok) wakeup_main(S_READY);

    return true;
}

size_t LMongo::busy_job(size_t *finished, size_t *unfinished)
{
    lock();
    size_t finished_sz   = _result.size();
    size_t unfinished_sz = _query.size();

    if (is_busy()) unfinished_sz += 1;
    unlock();

    if (finished) *finished = finished_sz;
    if (unfinished) *unfinished = unfinished_sz;

    return finished_sz + unfinished_sz;
}

void LMongo::routine(int32_t ev)
{
    UNUSED(ev);
    /* 如果某段时间连不上，只能由下次超时后触发
     * 超时时间由thread::start参数设定
     */
    if (ping()) return;

    lock();
    while (!_query.empty())
    {
        const struct MongoQuery *query = _query.front();
        _query.pop();

        unlock();
        struct MongoResult *res = do_command(query);
        delete query;
        lock();
        if (res)
        {
            _result.push(res);
            wakeup_main(S_DATA);
        }
    }
    unlock();
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
    lua_pushinteger(L, _dbid);

    if (LUA_OK != lua_pcall(L, 2, 0, 1))
    {
        ERROR("mongodb on ready error:%s", lua_tostring(L, -1));
        lua_pop(L, 1); /* remove error message */
    }

    lua_pop(L, 1);
}

void LMongo::main_routine(int32_t ev)
{
    static lua_State *L = StaticGlobal::state();

    if (EXPECT_FALSE(ev & S_READY)) on_ready(L);

    LUA_PUSHTRACEBACK(L);

    while (true)
    {
        lock();
        if (_result.empty())
        {
            unlock();
            break;
        }
        const struct MongoResult *res = _result.front();
        _result.pop();
        unlock();

        on_result(L, res);

        delete res;
    }

    lua_pop(L, 1); /* remove stacktrace */
}

void LMongo::push_query(const struct MongoQuery *query)
{
    lock();
    _query.push(query);
    wakeup(S_DATA);
    unlock();
}

int32_t LMongo::count(lua_State *L)
{
    if (!active())
    {
        return luaL_error(L, "mongo thread not active");
    }

    int32_t id             = luaL_checkinteger(L, 1);
    const char *collection = luaL_checkstring(L, 2);
    if (!collection)
    {
        return luaL_error(L, "mongo count:collection not specify");
    }

    const char *str_query = luaL_optstring(L, 3, NULL);
    const char *str_opts  = luaL_optstring(L, 4, NULL);

    bson_t *query = NULL;
    if (str_query)
    {
        bson_error_t error;
        query = bson_new_from_json(reinterpret_cast<const uint8_t *>(str_query),
                                   -1, &error);
        if (!query)
        {
            return luaL_error(L, error.message);
        }
    }

    bson_t *opts = NULL;
    if (str_opts)
    {
        bson_error_t error;
        opts = bson_new_from_json(reinterpret_cast<const uint8_t *>(str_opts),
                                  -1, &error);
        if (!opts)
        {
            bson_free(query);
            return luaL_error(L, error.message);
        }
    }

    struct MongoQuery *mongo_count = new MongoQuery();
    mongo_count->set(id, MQT_COUNT); /* count必须有返回 */
    mongo_count->set_count(collection, query, opts);

    push_query(mongo_count);

    return 0;
}

void LMongo::on_result(lua_State *L, const struct MongoResult *res)
{
    // 发起请求到返回主线程的时间，毫秒.thread是db线程耗时
    // 测试时发现数据库会有被饿死的情况，即主循环的消耗的时间很少，但db回调要很久才触发
    // 原因是ev那边频繁收到协议，导致数据库与子线程通信的fd一直没被epoll触发
    static AsyncLog *logger = StaticGlobal::async_logger();
    int64_t real            = StaticGlobal::ev()->ms_now() - res->_time;
    if (0 == res->_error.code)
    {
        logger->raw_write(
            "", LT_MONGODB, "%s.%s:%s real:" FMT64d " msec,thread:%.3f msec",
            res->_clt, MQT_NAME[res->_mqt], res->_query, real, res->_elaspe);
    }
    else
    {
        logger->raw_write(
            "", LT_MONGODB,
            "%s.%s:%s,code:%d,msg:%s,real:" FMT64d "  msec,thread:%.3f sec",
            res->_clt, MQT_NAME[res->_mqt], res->_query, res->_error.code,
            res->_error.message, real, res->_elaspe);

        ERROR("%s.%s:%s,code:%d,msg:%s,real:" FMT64d "  msec,thread:%.3f sec",
              res->_clt, MQT_NAME[res->_mqt], res->_query, res->_error.code,
              res->_error.message, real, res->_elaspe);
    }
    // 为0表示不需要回调到脚本
    if (0 == res->_qid) return;

    lua_getglobal(L, MONGODB_EVENT);

    lua_pushinteger(L, S_DATA);
    lua_pushinteger(L, _dbid);
    lua_pushinteger(L, res->_qid);
    lua_pushinteger(L, res->_error.code);

    int32_t nargs = 4;
    if (res->_data)
    {
        struct error_collector error;
        bson_type_t root_type =
            res->_mqt == MQT_FIND ? BSON_TYPE_ARRAY : BSON_TYPE_DOCUMENT;

        if (lbs_do_decode(L, res->_data, root_type, &error) < 0)
        {
            lua_pop(L, 4);
            ERROR("mongo result decode error:%s", error.what);

            // 即使出错，也回调到脚本
        }
        else
        {
            nargs++;
        }
    }

    if (LUA_OK != lua_pcall(L, nargs, 0, 1))
    {
        ERROR("mongo call back error:%s", lua_tostring(L, -1));
        lua_pop(L, 1); /* remove error message */
    }
}

/* 在子线程触发查询命令 */
MongoResult *LMongo::do_command(const struct MongoQuery *query)
{
    bool ok                 = false;
    auto begin              = std::chrono::steady_clock::now();
    struct MongoResult *res = new MongoResult();
    switch (query->_mqt)
    {
    case MQT_COUNT: ok = Mongo::count(query, res); break;
    case MQT_FIND: ok = Mongo::find(query, res); break;
    case MQT_FMOD: ok = Mongo::find_and_modify(query, res); break;
    case MQT_INSERT: ok = Mongo::insert(query, res); break;
    case MQT_UPDATE: ok = Mongo::update(query, res); break;
    case MQT_REMOVE: ok = Mongo::remove(query, res); break;
    default:
    {
        ERROR("unknow handle mongo command type:%d\n", query->_mqt);
        delete res;
        return nullptr;
    }
    }

    assert((ok && 0 == res->_error.code) || (!ok && 0 != res->_error.code));

    res->_qid  = query->_qid;
    res->_mqt  = query->_mqt;
    res->_time = query->_time;
    snprintf(res->_clt, MONGO_VAR_LEN, "%s", query->_clt);

    auto end = std::chrono::steady_clock::now();
    res->_elaspe =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();

    if (query->_query)
    {
        char *ctx = bson_as_json(query->_query, NULL);
        snprintf(res->_query, MONGO_VAR_LEN, "%s", ctx);
        bson_free(ctx);
    }

    return res;
}

// 把对应的json字符串或者lua table参数转换为bson
// @opt:可选参数：0 表示可以传入nil，返回NULL;1 表示未传入参数则创建一个新的bson
bson_t *LMongo::string_or_table_to_bson(lua_State *L, int index, int opt,
                                        bson_t *bs, ...)
{
#define CLEAN_BSON(arg)                                                          \
    do                                                                           \
    {                                                                            \
        va_list args;                                                            \
        for (va_start(args, arg); arg != END_BSON; arg = va_arg(args, bson_t *)) \
        {                                                                        \
            if (arg) bson_destroy(arg);                                          \
        }                                                                        \
        va_end(args);                                                            \
    } while (0)

    bson_t *bson = NULL;
    if (lua_istable(L, index)) // 自动将lua table 转化为bson
    {
        struct error_collector error;
        if (!(bson = lbs_do_encode(L, index, NULL, &error)))
        {
            CLEAN_BSON(bs);
            luaL_error(L, "table to bson error:%s", error.what);
            return NULL;
        }

        return bson;
    }

    if (lua_isstring(L, index)) // json字符串
    {
        const char *json = lua_tostring(L, index);
        bson_error_t error;
        bson = bson_new_from_json(reinterpret_cast<const uint8_t *>(json), -1,
                                  &error);
        if (!bson)
        {
            CLEAN_BSON(bs);
            luaL_error(L, "json to bson error:%s", error.message);
            return NULL;
        }

        return bson;
    }

    if (0 == opt) return NULL;
    if (1 == opt) return bson_new();

    luaL_error(L, "argument #%d expect table or json string", index);
    return NULL;

#undef CLEAN_BSON
}

/* find( id,collection,query,fields ) */
int32_t LMongo::find(lua_State *L)
{
    if (!active())
    {
        return luaL_error(L, "mongo thread not active");
    }

    int32_t id             = luaL_checkinteger(L, 1);
    const char *collection = luaL_checkstring(L, 2);
    if (!collection)
    {
        return luaL_error(L, "mongo find:collection not specify");
    }

    bson_t *query = string_or_table_to_bson(L, 3, 1);
    bson_t *opts  = string_or_table_to_bson(L, 4, 0, query, END_BSON);

    struct MongoQuery *mongo_find = new MongoQuery();
    mongo_find->set(id, MQT_FIND); /* count必须有返回 */
    mongo_find->set_find(collection, query, opts);

    push_query(mongo_find);

    return 0;
}

/* find( id,collection,query,sort,update,fields,remove,upsert,new ) */
int32_t LMongo::find_and_modify(lua_State *L)
{
    if (!active())
    {
        return luaL_error(L, "mongo thread not active");
    }

    int32_t id             = luaL_checkinteger(L, 1);
    const char *collection = luaL_checkstring(L, 2);
    if (!collection)
    {
        return luaL_error(L, "mongo find_and_modify:collection not specify");
    }

    bson_t *query  = string_or_table_to_bson(L, 3, 1);
    bson_t *sort   = string_or_table_to_bson(L, 4, 0, query, END_BSON);
    bson_t *update = string_or_table_to_bson(L, 5, 1, query, sort, END_BSON);
    bson_t *fields =
        string_or_table_to_bson(L, 6, 0, query, sort, update, END_BSON);

    bool _remove = lua_toboolean(L, 7);
    bool _upsert = lua_toboolean(L, 8);
    bool _new    = lua_toboolean(L, 9);

    struct MongoQuery *mongo_fmod = new MongoQuery();
    mongo_fmod->set(id, MQT_FMOD);
    mongo_fmod->set_find_modify(collection, query, sort, update, fields,
                                _remove, _upsert, _new);

    push_query(mongo_fmod);
    return 0;
}

/* insert( id,collections,info ) */
int32_t LMongo::insert(lua_State *L)
{
    if (!active())
    {
        return luaL_error(L, "mongo thread not active");
    }

    int32_t id             = luaL_checkinteger(L, 1);
    const char *collection = luaL_checkstring(L, 2);
    if (!collection)
    {
        return luaL_error(L, "mongo insert:collection not specify");
    }

    bson_t *query = string_or_table_to_bson(L, 3);

    struct MongoQuery *mongo_insert = new MongoQuery();
    mongo_insert->set(id, MQT_INSERT);
    mongo_insert->set_insert(collection, query);

    push_query(mongo_insert);

    return 0;
}

int32_t LMongo::update(lua_State *L)
{
    if (!active())
    {
        return luaL_error(L, "mongo thread not active");
    }

    int32_t id             = luaL_checkinteger(L, 1);
    const char *collection = luaL_checkstring(L, 2);
    if (!collection)
    {
        return luaL_error(L, "mongo update:collection not specify");
    }

    bson_t *query  = string_or_table_to_bson(L, 3);
    bson_t *update = string_or_table_to_bson(L, 4, -1, query, END_BSON);

    int32_t upsert = lua_toboolean(L, 5);
    int32_t multi  = lua_toboolean(L, 6);

    struct MongoQuery *mongo_update = new MongoQuery();
    mongo_update->set(id, MQT_UPDATE);
    mongo_update->set_update(collection, query, update, upsert, multi);

    push_query(mongo_update);

    return 0;
}

int32_t LMongo::remove(lua_State *L)
{
    if (!active())
    {
        return luaL_error(L, "mongo thread not active");
    }

    int32_t id             = luaL_checkinteger(L, 1);
    const char *collection = luaL_checkstring(L, 2);
    if (!collection)
    {
        return luaL_error(L, "mongo remove:collection not specify");
    }

    bson_t *query = string_or_table_to_bson(L, 3);

    int32_t single = lua_toboolean(L, 4);

    struct MongoQuery *mongo_remove = new MongoQuery();
    mongo_remove->set(id, MQT_REMOVE);
    mongo_remove->set_remove(collection, query, single);

    push_query(mongo_remove);

    return 0;
}
