-- MongoDB读写
local MongoDB = oo.class()

local Mongo = require "engine.Mongo"

--//////////////////////////////////////////////////////////////////////////////
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
    "find_and_modify",
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
