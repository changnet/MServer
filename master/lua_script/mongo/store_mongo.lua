-- store_mongo.lua
-- 2015-12-05
-- xzc

-- mongo db 数据存储

local Mongo = require "Mongo"
local Store_mongo = oo.class( nil,... )

function Store_mongo:__init()
    self._next_id = 0
    self._mongo = Mongo()

    self._mongo:self_callback( self )
    self._mongo:read_callback( self.on_result )
    self._mongo:error_callback( self.on_error )
end

--[[
底层id类型为int32，需要防止越界
]]
function Store_mongo:next_id()
    if self._next_id >= LIMIT.INT32_MAX then
        self._next_id = 0
    end

    self._next_id = self._next_id + 1
    return self._next_id
end

function Store_mongo:on_error()
    ELOG( "mongo db error" )
end

function Store_mongo:on_result()
end

function Store_mongo:start( ip,port,usr,pwd,db )
    self._mongo:start( ip,port,usr,pwd,db )
end

function Store_mongo:stop()
    self._mongo:stop()
    self._mongo:join()
end

function Store_mongo:count( collection,query,skip,limit )
    local id = self:next_id()
    return self._mongo:count( id,collection,query,skip,limit )
end

return Store_mongo
