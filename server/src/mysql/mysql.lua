-- mysql.lua
-- 2015-11-20
-- xzc
-- mysql数据存储处理(TODO: 提供一个转义接口，现在只是存日志，暂用不着)
local Sql = require "engine.Sql"
local mysql_mgr = require "mysql.mysql_mgr"
local AutoId = require "modules.system.auto_id"

local Mysql = oo.class(...)

function Mysql:__init(name)
    local id = mysql_mgr:next_id()

    self.id = id
    self.auto_id = AutoId()
    self.query = {}
    self.sql = Sql(id, name)
end

-- 连接成功回调
function Mysql:on_ready()
    if self.on_ready then self.on_ready() end
end

-- 收到db返回的数据
function Mysql:on_data(qid, ecode, res)
    local func = self.query[qid]
    if func then
        self.query[qid] = nil
        xpcall(func, __G__TRACKBACK, ecode, res)
    else
        elog("mysql result no call back found: db id = %d, query id = %d",
              self.id, qid)
    end
end

-- 异步连接mysql
-- @param ip 数据库地址
-- @param port 数据库端口
-- @param user 数据库用户名
-- @param pwd 数据库密码
-- @param db 数据库名
-- @param on_ready 连接成功回调
function Mysql:start(ip, port, usr, pwd, db, on_ready)
    self.on_ready = on_ready

    mysql_mgr:push(self)
    self.sql:start(ip, port, usr, pwd, db)
end

-- 关闭数据库连接
function Mysql:stop()
    mysql_mgr:pop(self.id)
    self.sql:stop()
end

-- 执行sql语句
function Mysql:exec_cmd(stmt)
    return self.sql:do_sql(0, stmt)
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
    return self.sql:do_sql(0, stmt)
end

function Mysql:update(stmt)
    return self.sql:do_sql(0, stmt)
end

function Mysql:call(stmt)
    -- 调用存储过程，未实现(注意分为有返回，无返回两种)
    assert(false)
end

-- 直接将lua table转换为sql，支持二进制及特殊字符
-- 如果fields为nil，则取values中的Key作为mysql字段名
function Mysql:insert_ex(tbl_name, values, fields)
    assert(false) -- 未实现
end

function Mysql:update_ex(tbl_name, values, fields)
    assert(false)
end

return Mysql
