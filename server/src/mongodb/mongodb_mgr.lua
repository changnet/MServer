-- mongodb_mgr.lua mongodb连接管理
-- 2016-05-22
-- xzc
-- mongodb连接管理
local MongodbMgr = oo.singleton(...)

local Mongo = require "engine.Mongo"

local S_READY = Mongo.S_READY
local S_DATA = Mongo.S_DATA

function MongodbMgr:__init()
    self.id = 0
    self.db = {}
end

-- 产生唯一的数据库id
function MongodbMgr:next_id()
    self.id = self.id + 1
    return self.id
end

function MongodbMgr:push(db)
    self.db[db.id] = db
end

function MongodbMgr:pop(id)
    self.db[id] = nil
end

local mgr = MongodbMgr()

function mongodb_event(ev, id, qid, ecode, res)
    local db = mgr.db[id]
    if not db then
        elogf("mongodb event no db found: id = %d", id)
        return
    end
    if ev == S_READY then
        db:on_ready()
    elseif ev == S_DATA then
        db:on_data(qid, ecode, res)
    else
        assert(false)
    end
end

return mgr

