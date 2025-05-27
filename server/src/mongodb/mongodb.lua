-- MongoDB读写
local MongoDB = oo.class()

local Mongo = require "engine.Mongo"

--//////////////////////////////////////////////////////////////////////////////
-- 这些定义的值要和mongo c driver(mongoc-flags.h)中的一致
-- 在C中的定义，前面都有MONGOC，如：REMOVE_NONE == MONGOC_REMOVE_NONE
MongoDB.REMOVE_NONE = 0
MongoDB.REMOVE_SINGLE_REMOVE = 1
MongoDB.UPDATE_NONE = 0
MongoDB.UPDATE_UPSERT = 1
MongoDB.UPDATE_MULTI_UPDATE = 2

local C_FUNC =
{
    "uriconnect",
    "disconnect",
    "ping",
    "error",
    "insert",
    "update",
    "count",
    "find",
    "remove",
    "set_array_opt",
    "find_and_modify",
    "drop_collection",
    "create_index",
    "drop_index",
}

oo.using(MongoDB, Mongo, function(name, func)
    return function(self, ...) return func(self.mongo, ...) end
end, C_FUNC)
--//////////////////////////////////////////////////////////////////////////////

function MongoDB:__init()
    self.mongo = Mongo()
end

function MongoDB:connect(host, port, user, passwd, database)
    -- mongodb://test_usr:test_pwd@127.0.0.1:27017/test_db
    return self.mongo:uriconnect(string.format(
        "mongodb://%s:%s@%s:%d/%s",
        user, passwd, host, port or 27017, database))
end

return MongoDB
