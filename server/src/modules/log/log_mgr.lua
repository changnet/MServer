-- log_mgr.lua
-- 2018-04-24
-- xzc
-- 日志管理

--[[
1. bulk insert，合并批量插入，注意语句不要超长了
insert into log value (1,2), (1,2)

SHOW VARIABLES LIKE 'max_allowed_packet';
max_allowed_packet=1048576 or 1 MiB

2. 使用事务，这种和bulk insert效率差不多，但需要3条指令。如果
start transaction;
insert into log value (1, 2)
insert into log value (1, 2)
insert into log value (1, 2)
insert into log value (1, 2)
commit;

3. load data直接从文件加载，这个应该是最快的
https://dev.mysql.com/doc/refman/8.0/en/load-data.html

load data infile "log.txt"

但是要考虑下转义的问题
https://dev.mysql.com/doc/refman/8.0/en/problems-with-null.html
When reading data with LOAD DATA, empty or missing columns are updated with ''. To load a NULL value into a column, use \N in the data file. The literal word NULL may also be used under some circumstances

从内存load data而不是从文件
https://mariadb.com/ja/resources/blog/customized-mysql-load-data-local-infile-handlers-with-libmysqlclient/

LOAD DATA INFILE is unsafe for statement-based replication
另外, load data是需要额外的权限的
]]

require "modules.log.log_header"

-- 日志模块
-- Log = {}

local Mysql = require "mysql.mysql"

local LogMgr = oo.singleton(...)

-- 初始化
function LogMgr:__init()
    -- 启动文件日志线程
    -- local async_file = Log()
    -- async_file:start( 3,0 ); -- 暂不启用另外的线程

    self.async_file = g_async_log -- async_file
end

-- 初始化db日志
function LogMgr:db_logger_init()
    self.db_logger = Mysql()
    self.db_logger:start(g_setting.mysql_ip, g_setting.mysql_port,
                         g_setting.mysql_user, g_setting.mysql_pwd,
                         g_setting.mysql_db, function()
        self:on_db_logger_init()
    end)
end

-- db日志初始化完成
function LogMgr:on_db_logger_init()
    g_app:one_initialized("db_logger", 1)
end

-- 关闭文件日志线程及数据库日志线程
function LogMgr:close()
    -- self.async_file:stop()
    if self.db_logger then self.db_logger:stop() end
end

-- 记录登录、退出日志
local login_out_stmt =
    "INSERT INTO `login_logout` (pid,op_type,op_time) values (%d,%d,%d)"
function LogMgr:login_or_logout(pid, op_type)
    self.db_logger:insert(string.format(login_out_stmt, pid, op_type, ev:time()))
end

-- 元宝操作日志
-- 日志里不应该有特殊字符，故ext不做特殊处理
function LogMgr:money_log(pid, id, op_val, new_val, op_type, ext)
    local stmt = string.format(
        "INSERT INTO `money` (pid,id,op_val,new_val,op_type,op_time,ext) \z
        values (%d,%d,%d,%d,%d,%d,\"%s\")", pid, id, op_val, new_val, op_type,
                     ev:time(), tostring(ext or ""))
    self.db_logger:insert(stmt)
end

-- 记录添加邮件操作日志
-- @who:玩家pid，如果是系统邮件，则为sys
function LogMgr:add_mail_log(who, mail)
    local ctx = string.format("add_mail:%s,%s", tostring(who), table.dump(mail))
    self.async_file:append_log_file("log/mail_log", ctx)
end

-- 记录删除邮件操作日志
-- @who:玩家pid，如果是系统邮件，则为sys
function LogMgr:del_mail_log(who, mail)
    local ctx = string.format("del_mail:%s,%s", tostring(who), table.dump(mail))
    self.async_file:append_log_file("log/mail_log", ctx)
end

-- 自定义写文件
function LogMgr:raw_file_printf(path, ...)
    return self.async_file:append_log_file(path, string.format(...))
end

local log_mgr = LogMgr()
return log_mgr
