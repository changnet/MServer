#include "lmongo.hpp"
#include <cstdarg>
#include <ctime> // for clock

#include "ltools.hpp"
#include <lbson.h>

#include "../system/static_global.hpp"

LMongo::LMongo(lua_State *L) : Thread("lmongo")
{
    _valid = -1;
    _dbid  = luaL_checkinteger(L, 2);
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

    _mongo.set(ip, port, usr, pwd, db);
    Thread::start(5);

    return 0;
}

int32_t LMongo::stop(lua_State *L)
{
    _valid = -1;
    Thread::stop();

    return 0;
}

// 该连接是否已连接上(不是当前状态，仅仅是第一次连接上)
int32_t LMongo::valid(lua_State *L)
{
    lua_pushinteger(L, _valid);

    return 1;
}

bool LMongo::initialize()
{
    int32_t ok = _mongo.connect();
    if (ok > 0)
    {
        _valid = 0;
        ERROR("mongo connect fail");
        return false;
    }
    else if (-1 == ok)
    {
        // 初始化正常，但是没ping通，需要重试
        return true;
    }

    _valid = 1;
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

void LMongo::routine()
{
    /* 如果某段时间连不上，只能由下次超时后触发
     * 超时时间由thread::start参数设定
     */
    if (_mongo.ping()) return;

    _valid = 1;
    invoke_command();
}

bool LMongo::uninitialize()
{
    if (_mongo.ping())
    {
        ERROR("mongo ping fail at cleanup,data may lost");
        /* TODO:write to file */
    }
    else
    {
        invoke_command();
    }

    _mongo.disconnect();

    return true;
}

void LMongo::notification(NotifyType notify)
{
    if (NT_CUSTOM == notify)
    {
        invoke_result();
    }
    else if (NT_ERROR == notify)
    {
        ERROR("mongo thread error");
    }
    else
    {
        assert(false);
    }
}

void LMongo::push_query(const struct MongoQuery *query)
{
    /* socket缓冲区大小是有限制的。如果query队列不为空，则表示已通知过子线程，无需再
     * 次通知。避免写入socket缓冲区满造成Resource temporarily unavailable
     */
    bool _notify = false;

    lock();
    if (_query.empty()) /* 不要使用_query.size() */
    {
        _notify = true;
    }
    _query.push(query);
    unlock();

    if (_notify) mark(S_SDATA);
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

const struct MongoResult *LMongo::pop_result()
{
    const struct MongoResult *res = NULL;

    lock();
    if (!_result.empty())
    {
        res = _result.front();
        _result.pop();
    }

    unlock();

    return res;
}

void LMongo::invoke_result()
{
    static lua_State *L = StaticGlobal::state();
    LUA_PUSHTRACEBACK(L);

    const struct MongoResult *res = NULL;
    while ((res = pop_result()))
    {
        // 发起请求到返回主线程的时间，毫秒.thread是db线程耗时
        // 测试时发现数据库会有被饿死的情况，即主循环的消耗的时间很少，但db回调要很久才触发
        // 原因是ev那边频繁收到协议，导致数据库与子线程通信的fd一直没被epoll触发
        static AsyncLog *logger = StaticGlobal::async_logger();
        int64_t real            = StaticGlobal::ev()->ms_now() - res->_time;
        if (0 == res->_error.code)
        {
            logger->raw_write("", LT_MONGODB,
                              "%s.%s:%s real:" FMT64d " msec,thread:%.3f sec",
                              res->_clt, MQT_NAME[res->_mqt], res->_query, real,
                              res->_elaspe);
        }
        else
        {
            logger->raw_write(
                "", LT_MONGODB,
                "%s.%s:%s,code:%d,msg:%s,real:" FMT64d "  msec,thread:%.3f sec",
                res->_clt, MQT_NAME[res->_mqt], res->_query, res->_error.code,
                res->_error.message, real, res->_elaspe);

            ERROR("%s.%s:%s,code:%d,msg:%s,real:" FMT64d
                  "  msec,thread:%.3f sec",
                  res->_clt, MQT_NAME[res->_mqt], res->_query, res->_error.code,
                  res->_error.message, real, res->_elaspe);
        }
        // 为0表示不需要回调到脚本
        if (0 == res->_qid)
        {
            delete res;
            continue;
        }

        lua_getglobal(L, "mongodb_read_event");

        lua_pushinteger(L, _dbid);
        lua_pushinteger(L, res->_qid);
        lua_pushinteger(L, res->_error.code);

        int32_t nargs = 3;
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

        delete res;
    }
    lua_pop(L, 1); /* remove stacktrace */
}

const struct MongoQuery *LMongo::pop_query()
{
    const struct MongoQuery *query = NULL;

    lock();
    if (!_query.empty())
    {
        query = _query.front();
        _query.pop();
    }

    unlock();

    return query;
}

/* 缓冲区大小有限，如果已经通知过了，则不需要重复通知 */
void LMongo::push_result(const struct MongoResult *result)
{
    bool is_notify = false;

    lock();
    if (_result.empty())
    {
        is_notify = true;
    }
    _result.push(result);
    unlock();

    if (is_notify) mark(S_MDATA);
}

/* 在子线程触发查询命令 */
void LMongo::invoke_command()
{
    const struct MongoQuery *query = NULL;
    while ((query = pop_query()))
    {
        bool ok                 = false;
        clock_t begin           = clock();
        struct MongoResult *res = new MongoResult();
        switch (query->_mqt)
        {
        case MQT_COUNT: ok = _mongo.count(query, res); break;
        case MQT_FIND: ok = _mongo.find(query, res); break;
        case MQT_FMOD: ok = _mongo.find_and_modify(query, res); break;
        case MQT_INSERT: ok = _mongo.insert(query, res); break;
        case MQT_UPDATE: ok = _mongo.update(query, res); break;
        case MQT_REMOVE: ok = _mongo.remove(query, res); break;
        default:
        {
            ERROR("unknow handle mongo command type:%d\n", query->_mqt);
            delete res;
            delete query;
            continue;
        }
        }

        if (ok)
        {
            assert(0 == res->_error.code);
        }
        else
        {
            assert(0 != res->_error.code);
        }

        res->_qid  = query->_qid;
        res->_mqt  = query->_mqt;
        res->_time = query->_time;
        snprintf(res->_clt, MONGO_VAR_LEN, "%s", query->_clt);
        res->_elaspe = ((float)(clock() - begin)) / CLOCKS_PER_SEC;

        if (query->_query)
        {
            char *ctx = bson_as_json(query->_query, NULL);
            snprintf(res->_query, MONGO_VAR_LEN, "%s", ctx);
            bson_free(ctx);
        }

        push_result(res);

        delete query;
    }
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

/* update( id,collections,query,update,upsert,multi ) */
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

/* remove( id,collections,query,multi ) */
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

    int32_t multi = lua_toboolean(L, 4);

    struct MongoQuery *mongo_remove = new MongoQuery();
    mongo_remove->set(id, MQT_REMOVE);
    mongo_remove->set_remove(collection, query, multi);

    push_query(mongo_remove);

    return 0;
}
