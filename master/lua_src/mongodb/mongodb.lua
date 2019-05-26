-- mongodb.lua
-- 2015-12-05
-- xzc

-- mongo db 数据存储

local Mongo = require "Mongo"
local Auto_id = require "modules.system.auto_id"

local Mongodb = oo.class( ... )

function Mongodb:__init( dbid )
    self.auto_id = Auto_id()
    self.mongodb = Mongo( dbid )

    self.cb = {}
end

-- 利用定时器来检测是否已连接上数据库
function Mongodb:do_timer()
    -- -1未连接或者连接中，0 连接失败，1 连接成功
    local ok = self.mongodb:valid()
    if -1 == ok then return end

    g_timer_mgr:del_timer( self.timer )
    self.timer = nil

    if 0 == ok then
        ERROR( "mongo db connect error" )
        return
    end

    if self.conn_cb then
        self.conn_cb()
        self.conn_cb = nil
    end
end

function Mongodb:read_event( qid,ecode,res )
    if self.cb[qid] then
        xpcall( self.cb[qid],__G__TRACKBACK__,ecode,res )
        self.cb[qid] = nil
    else
        ERROR( "mongo event no call back found" )
    end
end

-- 开始连接数据库，这个在另一个线程操作，是异步的，需要定时查询连接结果
function Mongodb:start( ip,port,usr,pwd,db,callback )
    self.mongodb:start( ip,port,usr,pwd,db )

    if callback then
        self.conn_cb = callback
        self.timer = g_timer_mgr:new_timer( 1,1,self,self.do_timer )
    end
end

function Mongodb:stop()
    return self.mongodb:stop()
end

function Mongodb:valid()
    return self.mongodb:valid()
end

function Mongodb:count( collection,query,skip,limit,callback )
    local id = self.auto_id:next_id( self.cb )
    self.cb[id] = callback
    return self.mongodb:count( id,collection,query,skip,limit )
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


-- >>>>>>>>>>>>>>>>>> coroutine 同步处理 <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
local Mongodb_sync = require "mongodb.mongodb_sync"

function Mongodb:new_sync( routine,... )
    local co = coroutine.create( routine )
    return Mongodb_sync( self,co )
end

-- 不提供索引函数，请开服使用脚本创建索引。见https://docs.mongodb.org/manual/reference/method/db.collection.createIndex/
--[[
db.collection.getIndexes() 查看已有索引
db.collection.dropIndex( name ) 删除索引
db.collection.createIndex( {amount:1} ) 创建索引
]]
return Mongodb
