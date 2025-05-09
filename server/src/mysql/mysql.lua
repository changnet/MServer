-- mysql.lua
-- 2015-11-20
-- xzc
-- mysql数据存储处理(TODO: 提供一个转义接口，现在只是存日志，暂用不着)
local Sql = require "engine.Sql"
local mysql_mgr = require "mysql.mysql_mgr"
local AutoId = require "modules.system.auto_id"

local Mysql = oo.class()

function Mysql:__init(name)
    local id = mysql_mgr:next_id()

    self.id = id
    self.auto_id = AutoId()
    self.query = {}
    self.sql = Sql(id, name)
end

-- 查询
-- @param stmt 需要执行的sql语句
-- @param on_select 查询回调函数
function Mysql:select(stmt, on_select)
    local id = self.auto_id:next_id(self.query)
    self.query[id] = function(ecode, res)
        return on_select(ecode, res)
    end

    self.sql:do_sql(id, stmt)
end

function Mysql:insert(stmt)
    -- INSERT INTO tbl_name (a,b,c) VALUES(1,2,3),(4,5,6),(7,8,9);
    -- INSERT INTO table (a,b,c) VALUES (1,2,3),(4,5,6)  ON DUPLICATE KEY UPDATE a = VALUES(a), b = VALUES(b);
    return self.sql:do_sql(0, stmt)
end

function Mysql:update(stmt)
    return self.sql:do_sql(0, stmt)
end

return Mysql
