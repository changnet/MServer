-- mongodb_mgr.lua mongodb连接管理
-- 2016-05-22
-- xzc

local Mongodb = require "mongodb.mongodb"
local MongodbMgr = oo.singleton( ... )

function MongodbMgr:__init()
    self.dbid = 0
    self.mongodb = {}
end

-- 新增db连接
function MongodbMgr:new()
    self.dbid = self.dbid + 1

     local mongodb = Mongodb( self.dbid )

     self.mongodb[self.dbid] = mongodb
     return mongodb
end

-- 断开所有db(阻塞)
function MongodbMgr:stop()
    for _,db in pairs( self.mongodb ) do
        db:stop()
    end
end

local db_mgr = MongodbMgr()

function mongodb_read_event( dbid,qid,ecode,res )
    local db = db_mgr.mongodb[dbid]
    db:read_event( qid,ecode,res )
end

return db_mgr

