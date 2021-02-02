-- mongodb.lua
-- 2015-12-05
-- xzc

-- mongodb 数据存储
local Mongodb = oo.class( ... )

local Mongo = require "Mongo"
local AutoId = require "modules.system.auto_id"
local mongodb_mgr = require "mongodb.mongodb_mgr"

function Mongodb:__init()
    self.id = mongodb_mgr:next_id()
    self.auto_id = AutoId()
    self.mongodb = Mongo(self.id)

    self.cb = {}
end


-- 连接成功回调
function Mongodb:on_ready()
    if self.on_ready then self.on_ready() end
end

function Mongodb:on_data( qid,ecode,res )
    if self.cb[qid] then
        xpcall( self.cb[qid],__G__TRACKBACK,ecode,res )
        self.cb[qid] = nil
    else
        ERROR( "mongo event no call back found" )
    end
end

-- 开始连接数据库
function Mongodb:start(ip, port, usr, pwd, db, on_ready)
    self.on_ready = on_ready

    mongodb_mgr:push(self)
    self.mongodb:start( ip,port,usr,pwd,db )
end

function Mongodb:stop()
    mongodb_mgr:pop(self.id)
    return self.mongodb:stop()
end

function Mongodb:count( collection,query,opts,callback )
    local id = self.auto_id:next_id( self.cb )
    self.cb[id] = callback
    return self.mongodb:count( id,collection,query,opts )
end

function Mongodb:find( collection,query,opts,callback )
    local id = self.auto_id:next_id( self.cb )
    self.cb[id] = callback
    return self.mongodb:find( id,collection,query,opts )
end

-- 查询对应的记录并修改
-- @query 查询条件
-- @sort 排序条件
-- @updata 更新条件
-- @fields 返回的字段
-- @remove 是否删除记录
-- @upsert 如果记录不存在则插入
-- @new 返回更改后的值
function Mongodb:find_and_modify(
    collection,query,sort,update,fields,remove,upsert,new,callback )

    local id = self.auto_id:next_id( self.cb )
    self.cb[id] = callback
    return self.mongodb:find_and_modify(
        id,collection,query,sort,update,fields,remove,upsert,new )
end

function Mongodb:insert( collection,info,callback )
    local id = 0
    if callback then
        id = self.auto_id:next_id( self.cb )
        self.cb[id] = callback
    end
    return self.mongodb:insert( id,collection,info )
end

function Mongodb:update( collection,query,info,upsert,multi,callback )
    local id = 0
    if callback then
        id = self.auto_id:next_id( self.cb )
        self.cb[id] = callback
    end
    return self.mongodb:update( id,collection,query,info,upsert,multi )
end

function Mongodb:remove( collection,query,multi,callback )
    local id = 0
    if callback then
        id = self.auto_id:next_id( self.cb )
        self.cb[id] = callback
    end
    return self.mongodb:remove( id,collection,query,multi )
end

-- 不提供索引函数，请开服使用脚本创建索引。
-- 见https://docs.mongodb.org/manual/reference/method/db.collection.createIndex/
--[[
db.collection.getIndexes() 查看已有索引
db.collection.dropIndex( name ) 删除索引
db.collection.createIndex( {amount:1} ) 创建索引
]]
return Mongodb
