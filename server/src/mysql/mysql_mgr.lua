-- mysql_mgr.lua
-- 2017-05-26
-- xzc

-- mysql连接管理

local Mysql = require "mysql.mysql"
local MysqlMgr = oo.singleton( ... )

function MysqlMgr:__init()
    self.dbid  = 0
    self.mysql = {}
end

function MysqlMgr:new()
    self.dbid = self.dbid + 1

    local mysql = Mysql( self.dbid )
    self.mysql[self.dbid] = mysql

    return mysql
end

function MysqlMgr:stop()
    for _,mysql in pairs( self.mysql ) do
        mysql:stop()
    end
    self.mysql = {}
end


local mysql_mgr = MysqlMgr()

--  底层回调很难回调到对应的mysql对象，只能在这里做一次分发
function mysql_read_event( dbid,qid,ecode,res )
    local mysql = mysql_mgr.mysql[dbid]
    mysql:read_event( qid,ecode,res )
end

return mysql_mgr
