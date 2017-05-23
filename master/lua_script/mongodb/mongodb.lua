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
function Mongodb:get_next_id()
    repeat
        if self.next_id >= LIMIT.INT32_MAX then self.next_id = 0 end

        self.next_id = self.next_id + 1
    until nil == self.cb[self.next_id]

    return self.next_id
end

function Mongodb:read_event( qid,ecode,res )
    if self.cb[id] then
        xpcall( self.cb[qid],__G__TRACKBACK__,ecode,res )
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

function Mongodb:count( this,method,collection,query,skip,limit )
    local id = self:get_next_id()
    self.cb[id] = function( ... ) return method( this,... ) end
    return self.mongodb:count( id,collection,query,skip,limit )
end

function Mongodb:find( this,method,collection,query,fields,skip,limit )
    local id = self:get_next_id()
    self.cb[id] = function( ... ) return method( this,... ) end
    return self.mongodb:find( id,collection,query,fields,skip,limit )
end

function Mongodb:find_and_modify( this,method,collection,query,opts )
    local id = self:get_next_id()
    self.cb[id] = function( ... ) return method( this,... ) end
    return self.mongodb:find_and_modify( id,collection,query,opts )
end

function Mongodb:insert( collection,info )
    return self.mongodb:insert( 0,collection,info )
end

function Mongodb:update( collection,query,info,upsert,multi )
    return self.mongodb:update( 0,collection,query,info,upsert,multi )
end

function Mongodb:remove( collection,query,multi )
    return self.mongodb:remove( 0,collection,query,multi )
end

-- 不提供索引函数，请开服使用脚本创建索引。见https://docs.mongodb.org/manual/reference/method/db.collection.createIndex/
--[[
db.collection.getIndexes() 查看已有索引
db.collection.dropIndex( name ) 删除索引
db.collection.createIndex( {amount:1} ) 创建索引
]]
return Mongodb
