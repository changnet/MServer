#include "mongo.hpp"

#include "lpp/lbson.hpp"

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
    array_opt_ = -1;
    clear_error();

    bson_init(&ping_);
    bson_append_int32(&ping_, "ping", -1, 1);

    client_   = nullptr;
    database_ = nullptr;
}

Mongo::~Mongo()
{
    disconnect();
}

void Mongo::clear_error()
{
    bson_set_error(&error_, 0, 0, "");
}

int32_t Mongo::uriconnect(lua_State *L)
{
    if (client_) return luaL_error(L, "already connect");

    const char *uri = luaL_checkstring(L, 2);

    mongoc_uri_t *m_uri = mongoc_uri_new_with_error(uri, &error_);
    if (!uri)
    {
        lua_pushinteger(L, error_.code);
        return 1;
    }

    client_ = mongoc_client_new_from_uri_with_error(m_uri, &error_);
    if (!client_)
    {
        lua_pushinteger(L, error_.code);
        return 1;
    }

    // 这个只是分配内存，并不会连接服务器
    database_ = mongoc_client_get_default_database(client_);
    // 目前的用法，连接时必须指定一个database;
    if (!database_) return luaL_error(L, "no database in uri");

    int32_t e = ping();
    lua_pushinteger(L, e);
    return 1;
}

void Mongo::disconnect()
{
    if (!client_) return;

    // http://mongoc.org/libmongoc/current/lifecycle.html#databases-collections-and-related-objects
    // Each of these objects must be destroyed before the client they were
    // created from, but their lifetimes are otherwise independent
    for (auto iter = collection_.begin(); iter != collection_.end(); ++iter)
    {
        mongoc_collection_destroy(iter->second);
    }
    collection_.clear();

    if (database_)
    {
        mongoc_database_destroy(database_);
        database_ = nullptr;
    }

    mongoc_client_destroy(client_);
    client_ = nullptr;
}

int32_t Mongo::ping()
{
    if (!client_) return -1;

    mongoc_cursor_t *cursor = mongoc_database_command(
        database_, (mongoc_query_flags_t)0, 0, 1, 0, &ping_, nullptr, nullptr);

    const bson_t *reply;
    if (!mongoc_cursor_next(cursor, &reply))
    {
        mongoc_cursor_error(cursor, &error_);
    }
    else
    {
        error_.code = 0;
    }

    mongoc_cursor_destroy(cursor); // cursor总是需要释放

    return error_.code;
}

int32_t Mongo::error(lua_State *L) const
{
    lua_pushinteger(L, error_.code);
    lua_pushstring(L, error_.message);
    return 2;
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

    auto iter = collection_.find(name);
    if (iter == collection_.end())
    {
        mongoc_collection_t *cl =
            mongoc_database_get_collection(database_, collection);
        collection_.emplace(name, cl);

        return cl;
    }

    return iter->second;
}

int32_t Mongo::count(lua_State *L)
{
    if (!client_) return luaL_error(L, "database not connect");

    const char *cl_name = luaL_checkstring(L, 2);
    bson_t *filter      = lbson::optbson(L, 3, array_opt_);
    bson_t *opts        = lbson::tobson(L, 4, array_opt_, filter);

    mongoc_collection_t *collection = get_collection(cl_name);

    // 如果失败，返回-1
    int64_t count = mongoc_collection_count_documents(collection, filter, opts,
                                                      nullptr, nullptr, &error_);

    lbson::bson_destory_list(filter, opts);
    lua_pushinteger(L, count);
    return 1;
}

int32_t Mongo::find(lua_State *L)
{
    if (!client_) return luaL_error(L, "database not connect");

    const char *cl_name = luaL_checkstring(L, 2);
    bson_t *filter      = lbson::optbson(L, 3, array_opt_);
    bson_t *opts        = lbson::tobson(L, 4, array_opt_, filter);

    mongoc_collection_t *collection = get_collection(cl_name);

    // http://mongoc.org/libmongoc/current/mongoc_collection_find_with_opts.html
    mongoc_cursor_t *cursor =
        mongoc_collection_find_with_opts(collection, filter, opts, nullptr);

    int32_t index = 0;
    bson_t *doc   = bson_new();

    const bson_t *sub_doc = nullptr;
    while (mongoc_cursor_next(cursor, &sub_doc))
    {
        /**
         * bson_append_document内部使用memcpy做了内存拷贝,无需使用bson_copy
         * array、document是一样的，只是前者的key必须为0 1 2 ...,使用
         * bson_append_document还是bson_append_array仅在bson_iter_type中有区别
         */

        // TODO 这里每次调用snprintf效率是不是有点低，要不要做缓存
        // TODO 直接在lua栈上创建一个table是否可行？不用append_document
        char key[128];
        int32_t key_len = (int32_t)snprintf(key, sizeof(key), "%d", index);
        assert(key_len < sizeof(key));

        bool r = bson_append_document(doc, key, key_len, sub_doc);
        index++;

        assert(r);
        UNUSED(r);
    }

    int32_t e = 0;
    if (mongoc_cursor_error(cursor, &error_))
    {
        bson_destroy(doc);
        e = error_.code;
    }

    mongoc_cursor_destroy(cursor);
    lbson::bson_destory_list(filter, opts);

    lua_pushinteger(L, e);
    if (e) return 1;

    e = lbson::decode(L, doc, &error_, BSON_TYPE_ARRAY, array_opt_);
    bson_destroy(doc);
    if (e)
    {
        lua_pop(L, 1); // pop old error
        lua_pushinteger(L, error_.code);
        return 1;
    }

    return 2;
}

int32_t Mongo::find_and_modify(lua_State *L)
{
    if (!client_) return luaL_error(L, "database not connect");

    const char *cl_name = luaL_checkstring(L, 2);

    bson_t *query  = lbson::optbson(L, 3, array_opt_);
    bson_t *sort   = lbson::optbson(L, 4, array_opt_, query);
    bson_t *update = lbson::optbson(L, 5, array_opt_, query, sort);
    bson_t *fields = lbson::optbson(L, 6, array_opt_, query, sort, update);
    bool remove    = lua_toboolean(L, 7);
    bool upsert    = lua_toboolean(L, 8);
    bool ret_new   = lua_toboolean(L, 9);

    mongoc_collection_t *collection = get_collection(cl_name);

    bson_t *reply = bson_new();
    // http://mongoc.org/libmongoc/current/mongoc_find_and_modify_opts_t.html#functions
    // mongoc_find_and_modify_opts的功能和这一样，只不过使用了opts参数，参数显示简洁一些
    // 不过需要额外构建一个mongoc_find_and_modify_opts_t类型
    bool ok = mongoc_collection_find_and_modify(collection, query, sort, update,
                                                fields, remove, upsert, ret_new,
                                                reply, &error_);
    int32_t e = 0;
    lua_pushinteger(L, ok ? 0 : error_.code);
    if (ok)
    {
        e = lbson::decode(L, reply, &error_, BSON_TYPE_ARRAY, array_opt_);
        if (e)
        {
            lua_pop(L, 1); // pop old error
            lua_pushinteger(L, error_.code);
        }
    }
    lbson::bson_destory_list(query, sort, update, fields, reply);

    return e ? 1 : 2;
}

int32_t Mongo::insert(lua_State *L)
{
    if (!client_) return luaL_error(L, "database not connect");

    const char *cl_name = luaL_checkstring(L, 2);
    bson_t *document    = lbson::checkbson(L, 3, array_opt_);

    mongoc_collection_t *collection = get_collection(cl_name);
    bool ok = mongoc_collection_insert(collection, MONGOC_INSERT_NONE, document,
                                       nullptr, &error_);

    bson_destroy(document);
    lua_pushinteger(L, ok ? 0 : error_.code);
    return 1;
}

int32_t Mongo::update(lua_State *L)
{
    if (!client_) return luaL_error(L, "database not connect");

    const char *cl_name = luaL_checkstring(L, 2);
    int32_t flags       = luaL_checkinteger32(L, 3);
    bson_t *selector    = lbson::checkbson(L, 3, array_opt_);
    bson_t *update      = lbson::checkbson(L, 4, array_opt_, selector);

    mongoc_collection_t *collection = get_collection(cl_name);
    bool ok = mongoc_collection_update(collection, (mongoc_update_flags_t)flags,
                                       selector, update, nullptr, &error_);

    lbson::bson_destory_list(selector, update);
    lua_pushinteger(L, ok ? 0 : error_.code);
    return 1;
}

int32_t Mongo::remove(lua_State *L)
{
    if (!client_) return luaL_error(L, "database not connect");

    const char *cl_name             = luaL_checkstring(L, 2);
    int32_t flags                   = luaL_checkinteger32(L, 3);
    bson_t *selector                = lbson::checkbson(L, 3, array_opt_);
    mongoc_collection_t *collection = get_collection(cl_name);

    bool ok = mongoc_collection_remove(collection, (mongoc_remove_flags_t)flags,
                                       selector, nullptr, &error_);

    bson_destroy(selector);
    lua_pushinteger(L, ok ? 0 : error_.code);
    return 1;
}
