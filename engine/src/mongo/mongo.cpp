#include "mongo.hpp"

void Mongo::init()
{
    mongoc_init();
}

void Mongo::cleanup()
{
    mongoc_cleanup();
}

Mongo::Mongo()
{
    _conn = nullptr;
}

Mongo::~Mongo()
{
    assert(nullptr == _conn);
}

void Mongo::set(const char *ip, const int32_t port, const char *usr,
                const char *pwd, const char *db)
{
    /* 将数据复制一份，允许上层释放对应的内存 */
    _port = port;
    snprintf(_ip, MONGO_VAR_LEN, "%s", ip);
    snprintf(_usr, MONGO_VAR_LEN, "%s", usr);
    snprintf(_pwd, MONGO_VAR_LEN, "%s", pwd);
    snprintf(_db, MONGO_VAR_LEN, "%s", db);
}

int32_t Mongo::connect()
{
    assert(!_conn);

    char uri[PATH_MAX];
    /* "mongodb://user:password@localhost/?authSource=mydb" */
    snprintf(uri, PATH_MAX, "mongodb://%s:%s@%s:%d/?authSource=%s", _usr, _pwd,
             _ip, _port, _db);
    _conn = mongoc_client_new(uri);
    if (!_conn)
    {
        ERROR("parse mongo uri fail\n");
        return 1;
    }

    /* mongoc_client_new只是创建一个对象，并没有connect,ping保证连接通畅
     * 默认10s超时.超服时阻塞,应该可以接受.尝试ping一下，失败也不一定说明有问题
     * 需要关注日志。
     */
    return 0;
}

void Mongo::disconnect()
{
    if (_conn)
    {
        // http://mongoc.org/libmongoc/current/lifecycle.html#databases-collections-and-related-objects
        // Each of these objects must be destroyed before the client they were created from, but their lifetimes are otherwise independent
        for (auto iter = _collection.begin(); iter != _collection.end(); ++ iter)
        {
            mongoc_collection_destroy(iter->second);
        }
        _collection.clear();
        mongoc_client_destroy(_conn);
    }
    _conn = nullptr;
}

int32_t Mongo::ping()
{
    assert(_conn);

    bson_t ping;
    bson_init(&ping);
    bson_append_int32(&ping, "ping", -1, 1);
    mongoc_database_t *database = mongoc_client_get_database(_conn, _db);

    /* cursor总是需要释放 */
    mongoc_cursor_t *cursor = mongoc_database_command(
        database, (mongoc_query_flags_t)0, 0, 1, 0, &ping, nullptr, nullptr);

    const bson_t *reply;
    if (mongoc_cursor_next(cursor, &reply))
    {
        mongoc_cursor_destroy(cursor);
        bson_destroy(&ping);

        mongoc_database_destroy(database);
        return 0;
    }

    /* get the error */
    bson_error_t error;
    int32_t ecode = mongoc_cursor_error(cursor, &error);
    if (ecode)
    {
        ERROR_R("mongo ping error(%d):%s", error.code, error.message);
    }

    mongoc_cursor_destroy(cursor);
    bson_destroy(&ping);
    mongoc_database_destroy(database);

    return ecode;
}

mongoc_collection_t *Mongo::get_collection(const char *collection)
{
    // http://mongoc.org/libmongoc/current/lifecycle.html#databases-collections-and-related-objects
    // Each of these objects must be destroyed before the client they were
    // created from, but their lifetimes are otherwise independent
    // 没有具体提到mongoc_collection_t能不能缓存，应该是可以的
    // 以前做过一个版本是每次查询都创建、销毁一个mongoc_collection_t
    thread_local std::string name;
    name.assign(collection);

    auto iter = _collection.find(name);
    if (iter == _collection.end())
    {
        mongoc_collection_t *clt =
            mongoc_client_get_collection(_conn, _db, collection);
        _collection.emplace(name, clt);

        return clt;
    }

    return iter->second;
}

bool Mongo::count(const MongoQuery *mq, MongoResult *res)
{
    assert(mq);
    assert(_conn);

    mongoc_collection_t *collection = get_collection(mq->_clt);

    // opts = {"skip":1,"limit":5}
    int64_t count = mongoc_collection_count_documents(
        collection, mq->_query, mq->_opts, nullptr, nullptr, &res->_error);

    if (count < 0) /* 如果失败，返回-1 */
    {
        res->_data = nullptr;

        return false;
    }

    bson_t *doc = bson_new();
    BSON_APPEND_INT64(doc, "count", count);

    res->_data = doc;

    return true;
}

bool Mongo::find(const MongoQuery *mq, MongoResult *res)
{
    assert(mq);
    assert(_conn);

    mongoc_collection_t *collection = get_collection(mq->_clt);

    // http://mongoc.org/libmongoc/current/mongoc_collection_find_with_opts.html
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        collection, mq->_query, mq->_opts, nullptr);

    int32_t index = 0;
    bson_t *doc   = bson_new();

    const bson_t *sub_doc = nullptr;
    while (mongoc_cursor_next(cursor, &sub_doc))
    {
        /* The bson objects set in this function are ephemeral and good until
         * the next call. This means that you must copy the returned bson if you
         * wish to retain it beyond the lifetime of a single call to
         * mongoc_cursor_next()
         * bson_append_document内部使用memcpy做了内存拷贝,无需使用bson_copy
         * 对bson，array、document是一样的，只是前者的key必须为0 1 2 ...,使用
         * bson_append_document还是bson_append_array仅在bson_iter_type中有区别
         */

        const int32_t buff_len = 128;
        char index_buff[buff_len];
        snprintf(index_buff, buff_len, "%d", index);

        bool r = BSON_APPEND_DOCUMENT(doc, index_buff, sub_doc);
        index++;

        assert(r);
        UNUSED(r);
    }

    if (mongoc_cursor_error(cursor, &res->_error))
    {
        ERROR_R("mongoc_cursor_error");
        bson_destroy(doc);
        res->_data = nullptr;

        return false;
    }

    res->_data = doc;

    mongoc_cursor_destroy(cursor);

    return true;
}

bool Mongo::find_and_modify(const MongoQuery *mq, MongoResult *res)
{
    assert(mq);
    assert(_conn);

    mongoc_collection_t *collection =
        get_collection(mq->_clt);

    assert(nullptr == res->_data);

    res->_data = bson_new();
    // http://mongoc.org/libmongoc/current/mongoc_find_and_modify_opts_t.html#functions
    // mongoc_find_and_modify_opts的功能和这一样，只不过使用了opts参数，参数显示简洁一些
    // 不过需要额外构建一个mongoc_find_and_modify_opts_t类型
    bool ok    = mongoc_collection_find_and_modify(
        collection, mq->_query, mq->_sort, mq->_update, mq->_fields,
        mq->_remove, mq->_upsert, mq->_new, res->_data, &res->_error);

    if (!ok)
    {
        bson_destroy(res->_data);
        res->_data = nullptr;

        return false;
    }

    return true;
}

bool Mongo::insert(const MongoQuery *mq, MongoResult *res)
{
    assert(mq);
    assert(_conn);

    mongoc_collection_t *collection = get_collection(mq->_clt);

    bool ok = mongoc_collection_insert(collection, MONGOC_INSERT_NONE,
                                       mq->_query, nullptr, &res->_error);

    return ok;
}

bool Mongo::update(const MongoQuery *mq, MongoResult *res)
{
    assert(mq);
    assert(_conn);

    mongoc_collection_t *collection = get_collection(mq->_clt);

    mongoc_update_flags_t flags = (mongoc_update_flags_t)mq->_flags;

    bool ok = mongoc_collection_update(collection, flags, mq->_query,
                                       mq->_update, nullptr, &res->_error);

    return ok;
}

bool Mongo::remove(const MongoQuery *mq, MongoResult *res)
{
    assert(mq);
    assert(_conn);

    mongoc_collection_t *collection = get_collection(mq->_clt);

    bool ok =
        mongoc_collection_remove(collection, (mongoc_remove_flags_t)mq->_flags,
                                 mq->_query, nullptr, &res->_error);

    return ok;
}
