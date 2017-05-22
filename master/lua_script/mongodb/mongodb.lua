-- mongodb.lua
-- 2015-12-05
-- xzc

-- mongo db 数据存储

local Mongo = require "Mongo"
local LIMIT = require "global.limits"

local Mongodb = oo.class( nil,... )

function Mongodb:__init( dbid )
    self.next_id = 0
    self.mongodb = Mongo( dbid )

    self.cb = {}
end

--[[
底层id类型为int32，需要防止越界
]]
function Mongodb:next_id()
    repeat
        if self.next_id >= LIMIT.INT32_MAX then self.next_id = 0 end

        self.next_id = self.next_id + 1
    while nil == self.cb[self.next_id]

    return self.next_id
end

function Mongodb:error_event()
    ELOG( "MONGO DB ERROR" )
end

function Mongodb:read_event()
    local id,err,info = self.mongodb:next_result()
    if self.cb[id] then
        xpcall( self.cb[id],__G__TRACKBACK__,err,info )
    else
        ELOG( "mongo result no call back found" )
    end
end

function Mongodb:start( ip,port,usr,pwd,db )
    return self.mongodb:start( ip,port,usr,pwd,db )
end

function Mongodb:stop()
    return self.mongodb:stop()
end

function Mongodb:count( cb,collection,query,skip,limit )
    local id = self:next_id()
    self.cb[id] = cb
    return self.mongodb:count( id,collection,query,skip,limit )
end

function Mongodb:find( cb,collection,query,fields,skip,limit )
    local id = self:next_id()
    self.cb[id] = cb
    return self.mongodb:find( id,collection,query,fields,skip,limit )
end

function Mongodb:find_and_modify( cb,collection,query,sort,fields,remove,upsert,new )
    local id = self:next_id()
    self.cb[id] = cb
    return self.mongodb:find_and_modify( id,collection,query,sort,fields,remove,upsert,new )
end

function Mongodb:insert( collection,info )
    local id = self:next_id()
    return self.mongodb:insert( id,collection,info )
end

function Mongodb:update( collection,query,info,upsert,multi )
    local id = self:next_id()
    return self.mongodb:update( id,collection,query,info,upsert,multi )
end

function Mongodb:remove( collection,query,multi )
    local id = self:next_id()
    return self.mongodb:remove( id,collection,query,multi )
end

-- 不提供索引函数，请开服使用脚本创建索引。见https://docs.mongodb.org/manual/reference/method/db.collection.createIndex/
--[[
db.collection.getIndexes() 查看已有索引
db.collection.dropIndex( name ) 删除索引
db.collection.createIndex( {amount:1} ) 创建索引
]]
return Mongodb
