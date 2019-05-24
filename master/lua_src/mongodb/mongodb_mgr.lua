-- mongodb_mgr.lua mongodb连接管理
-- 2016-05-22
-- xzc

local Mongodb = require "mongodb.mongodb"
local Mongodb_mgr = oo.singleton( ... )

function Mongodb_mgr:__init()
    self.dbid = 0
    self.mongodb = {}
end

-- 新增db连接
function Mongodb_mgr:new()
    self.dbid = self.dbid + 1

     local mongodb = Mongodb( self.dbid )

     self.mongodb[self.dbid] = mongodb
     return mongodb
end

-- 断开所有db(阻塞)
function Mongodb_mgr:stop()
    for _,db in pairs( self.mongodb ) do
        db:stop()
    end
end

local db_mgr = Mongodb_mgr()

function mongodb_read_event( dbid,qid,ecode,res )
    local db = db_mgr.mongodb[dbid]
    db:read_event( qid,ecode,res )
end

return db_mgr

