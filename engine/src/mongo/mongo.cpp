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

    // C++14以后，支持透明对比(transparent comparison)，允许std::string和const char *直接对比
    // 不会构建一个std::string来对比
    auto iter = collection_.find(collection);
    if (iter == collection_.end())
    {
        // 这个只是分配内存，不存在失败
        mongoc_collection_t *cl =
            mongoc_database_get_collection(database_, collection);
        collection_.emplace(collection, cl);

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

    int32_t e = 0;
    int32_t index = 0;
    const bson_t *doc = nullptr;

    lua_newtable(L);
    while (mongoc_cursor_next(cursor, &doc))
    {
        /**
         * bson_append_document内部使用memcpy做了内存拷贝,无需使用bson_copy
         * array、document是一样的，只是前者的key必须为0 1 2 ...,使用
         * bson_append_document还是bson_append_array仅在bson_iter_type中有区别
         */

        int32_t e = lbson::decode(L, doc, &error_, BSON_TYPE_DOCUMENT, array_opt_);
        if (e)
        {
            lua_pop(L, 1); // pop result table
            break;
        }

        index++;
        lua_rawseti(L, -2, index);
    }

    if (!e && mongoc_cursor_error(cursor, &error_))
    {
        lua_pop(L, 1); // pop result table
        e = error_.code;
    }

    mongoc_cursor_destroy(cursor);
    lbson::bson_destory_list(filter, opts);

    lua_pushinteger(L, e);
    if (e) return 1;

    lua_insert(L, -2); // insert error code before table
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

    /*
    https://www.mongodb.com/docs/languages/c/c-driver/current/libbson/guides/lifetimes/

    Warning
    Passing a bson_t pointer obtained from bson_new as an out parameter will
    result in a leak of the bson_t struct.

    bson_t *heap_doc = bson_new ();
    example_get_doc (heap_doc);
    bson_destroy (heap_doc); // Leaks the `bson_t` struct!

    bson_destroy只销毁子document，通过bson_new来创建是需要bson_free来销毁的？？
    但其他地方为什么都是只用bson_destroy
    */
    bson_t reply = BSON_INITIALIZER;

    // http://mongoc.org/libmongoc/current/mongoc_find_and_modify_opts_t.html#functions
    // mongoc_find_and_modify_opts的功能和这一样，只不过使用了opts参数，参数显示简洁一些
    // 不过需要额外构建一个mongoc_find_and_modify_opts_t类型
    bool ok = mongoc_collection_find_and_modify(collection, query, sort, update,
                                                fields, remove, upsert, ret_new,
                                                &reply, &error_);
    int32_t e = 0;
    lua_pushinteger(L, ok ? 0 : error_.code);
    if (ok)
    {
        e = lbson::decode(L, &reply, &error_, BSON_TYPE_DOCUMENT, -1);
        if (e)
        {
            lua_pop(L, 1); // pop old error
            lua_pushinteger(L, error_.code);
        }
    }
    lbson::bson_destory_list(query, sort, update, fields, &reply);

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

int32_t Mongo::insert_many(lua_State *L)
{
    if (!client_) return luaL_error(L, "database not connect");

    if (!lua_istable(L, 3))
    {
        return luaL_error(L, "expect table at #3, got %s",
                          lua_typename(L, lua_type(L, 3)));
    }
    const char *cl_name = luaL_checkstring(L, 2);

    bool ok            = true;
    size_t n_documents = 0;
    bson_t *documents[1024];
    int32_t top = lua_gettop(L);

    bson_t **docs = documents;
    size_t size_docs = std::size(documents);

    // 允许传入hash table，无法预知数量
    lua_pushnil(L);
    while (lua_next(L, 3))
    {
        bson_t *b = lbson::encode(L, top + 2, &error_, array_opt_);
        if (!b)
        {
            ok = false;
            lua_pop(L, 2);
            goto RETURN;
        }

        docs[n_documents] = b;
        n_documents++;
        if (n_documents == size_docs)
        {
            bson_t **new_docs = new bson_t *[size_docs * 2];
            memcpy(new_docs, docs, sizeof(bson_t *) * n_documents);

            if (docs != documents) delete[] docs;
            docs = new_docs;
            size_docs = size_docs * 2;
        }
        lua_pop(L, 1);
    }

    mongoc_collection_t *collection = get_collection(cl_name);
    ok = mongoc_collection_insert_many(collection, (const bson_t **)docs,
                                       n_documents, nullptr, nullptr, &error_);

RETURN:
    for (size_t i = 0; i < n_documents; i++)
    {
        bson_destroy(docs[i]);
    }
    if (docs != documents) delete[] docs;

    lua_pushinteger(L, ok ? 0 : error_.code);
    return 1;
}

int32_t Mongo::update(lua_State *L)
{
    if (!client_) return luaL_error(L, "database not connect");

    const char *cl_name = luaL_checkstring(L, 2);
    int32_t flags       = luaL_checkinteger32(L, 3);
    bson_t *selector    = lbson::checkbson(L, 4, -1);
    bson_t *update      = lbson::checkbson(L, 5, array_opt_, selector);

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
    bson_t *selector                = lbson::checkbson(L, 4, array_opt_);

    mongoc_collection_t *collection = get_collection(cl_name);
    bool ok = mongoc_collection_remove(collection, (mongoc_remove_flags_t)flags,
                                       selector, nullptr, &error_);

    bson_destroy(selector);
    lua_pushinteger(L, ok ? 0 : error_.code);
    return 1;
}

int32_t Mongo::drop_collection(lua_State *L)
{
    if (!client_) return luaL_error(L, "database not connect");

    const char *cl_name = luaL_checkstring(L, 2);
    mongoc_collection_t *collection = get_collection(cl_name);

    bool ok = mongoc_collection_drop(collection, &error_);

    lua_pushinteger(L, ok ? 0 : error_.code);
    return 1;
}

int32_t Mongo::drop_index(lua_State* L)
{
    if (!client_) return luaL_error(L, "database not connect");

    const char *cl_name = luaL_checkstring(L, 2);
    const char *index_name = luaL_checkstring(L, 3);

    mongoc_collection_t *collection = get_collection(cl_name);
    bool ok = mongoc_collection_drop_index(collection, index_name, &error_);

    lua_pushinteger(L, ok ? 0 : error_.code);
    return 1;
}

int32_t Mongo::create_index(lua_State *L)
{
    if (!client_) return luaL_error(L, "database not connect");

    const char *cl_name = luaL_checkstring(L, 2);
    bson_t *keys        = lbson::checkbson(L, 3, -1);
    bson_t *opts        = lbson::tobson(L, 4, -1, keys);

    int32_t e                         = 0;
    bson_t reply                      = BSON_INITIALIZER;
    mongoc_collection_t *collection   = get_collection(cl_name);
    mongoc_index_model_t *index_model = mongoc_index_model_new(keys, opts);
    if (!mongoc_collection_create_indexes_with_opts(collection, &index_model, 1,
                                                    nullptr, &reply, &error_))
    {
        e = error_.code;
    }

    lua_pushinteger(L, e);
    if (!e)
    {
        e = lbson::decode(L, &reply, &error_, BSON_TYPE_DOCUMENT, -1);
        if (e)
        {
            lua_pop(L, 1); // pop old error
            lua_pushinteger(L, error_.code);
        }
    }

    lbson::bson_destory_list(keys, opts, &reply);
    mongoc_index_model_destroy(index_model);

    return e ? 1 : 2;
}

int32_t Mongo::aggregate(lua_State *L)
{
    if (!client_) return luaL_error(L, "database not connect");

    const char *cl_name = luaL_checkstring(L, 2);
    bson_t *pipeline      = lbson::optbson(L, 3, -1);

    mongoc_collection_t *collection = get_collection(cl_name);
    mongoc_cursor_t *cursor = mongoc_collection_aggregate(
        collection, MONGOC_QUERY_NONE, pipeline, nullptr, nullptr);

    int32_t e         = 0;
    int32_t index     = 0;
    const bson_t *doc = nullptr;

    lua_newtable(L);
    while (mongoc_cursor_next(cursor, &doc))
    {
        int32_t e =
            lbson::decode(L, doc, &error_, BSON_TYPE_DOCUMENT, array_opt_);
        if (e)
        {
            lua_pop(L, 1); // pop result table
            break;
        }

        index++;
        lua_rawseti(L, -2, index);
    }

    if (!e && mongoc_cursor_error(cursor, &error_))
    {
        lua_pop(L, 1); // pop result table
        e = error_.code;
    }

    mongoc_cursor_destroy(cursor);
    bson_destroy(pipeline);

    lua_pushinteger(L, e);
    if (e) return 1;

    lua_insert(L, -2); // insert error code before table
    return 2;
}
