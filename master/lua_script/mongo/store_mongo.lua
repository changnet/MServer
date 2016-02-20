-- store_mongo.lua
-- 2015-12-05
-- xzc

-- mongo db 数据存储

local Mongo = require "Mongo"
local LIMIT = require "global.limits"

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
lua至少为float
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
    local id,err,info = self._mongo:next_result()
    print( "mongo result ",id,err )
    vd( info )
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

function Store_mongo:find( collection,query,fields,skip,limit )
    local id = self:next_id()
    return self._mongo:find( id,collection,query,fields,skip,limit )
end

function Store_mongo:find_and_modify( collection,query,sort,fields,remove,upsert,new )
    local id = self:next_id()
    return self._mongo:find_and_modify( id,collection,query,sort,fields,remove,upsert,new )
end

function Store_mongo:insert( collection,info )
    local id = self:next_id()
    return self._mongo:insert( id,collection,info )
end

function Store_mongo:update( collection,query,info,upsert,multi )
    local id = self:next_id()
    return self._mongo:update( id,collection,query,info,upsert,multi )
end

function Store_mongo:remove( collection,query,multi )
    local id = self:next_id()
    return self._mongo:remove( id,collection,query,multi )
end

-- 不提供索引函数，请开服使用脚本创建索引。见https://docs.mongodb.org/manual/reference/method/db.collection.createIndex/
return Store_mongo
