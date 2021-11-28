-- mongodb_mgr.lua mongodb连接管理
-- 2016-05-22
-- xzc
-- mongodb连接管理
local MongoDBMgr = {}

local this = global_storage("MongoDBMgr", {
    id = 0,
    db = {},
})

local Mongo = require "engine.Mongo"

local S_READY = Mongo.S_READY
local S_DATA = Mongo.S_DATA

-- 产生唯一的数据库id
function MongoDBMgr.next_id()
    this.id = this.id + 1
    return this.id
end

function MongoDBMgr.push(db)
    this.db[db.id] = db
end

function MongoDBMgr.pop(id)
    this.db[id] = nil
end

function mongodb_event(ev, id, qid, ecode, res)
    local db = this.db[id]
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

return MongoDBMgr

