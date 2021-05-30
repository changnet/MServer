-- mysql_mgr.lua
-- 2017-05-26
-- xzc
-- mysql连接管理
local MysqlMgr = oo.singleton(...)

local Sql = require "Sql"

local S_READY = Sql.S_READY
local S_DATA = Sql.S_DATA

function MysqlMgr:__init()
    self.id = 0
    self.sql = {}
end

-- 产生一个唯一db id
function MysqlMgr:next_id()
    self.id = self.id + 1

    return self.id
end

function MysqlMgr:push(db)
    self.sql[db.id] = db
end

function MysqlMgr:pop(id)
    self.sql[id] = nil
end

local mysql_mgr = MysqlMgr()

-- 底层回调无法回调到对应的mysql对象，只能在这里做一次分发
function mysql_event(ev, id, qid, ecode, res)
    local sql = mysql_mgr.sql[id]
    if not sql then
        ERROR("mysql event no sql found: id = %d", id)
        return
    end
    if ev == S_READY then
        sql:on_ready()
    elseif ev == S_DATA then
        sql:on_data(qid, ecode, res)
    else
        assert(false)
    end
end

return mysql_mgr
