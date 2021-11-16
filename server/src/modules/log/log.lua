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
local Mysql = require "mysql.mysql"

-- 日志模块
Log = {}

local this = global_storage("Log", {
    cache = {}
})

-- 异步文件写入线程
local async_file = g_async_log

-- 写入到日志文件
function Log.file(path, text)
end

-- 记录资源日志
function Log.db_res(player, id, change, count, op, msg)
end

-- 记录状态status到数据库，只记录一个，如果存在则覆盖
function Log.db_stat(pid, stat, val)
end

-- 记录一些杂项日志到数据库
function Log.db_misc(player, op, msg)
end

-- 记录日志到数据库(无玩家对象)
function Log.db_pid_misc(pid, id, val, op, msg)
end

-- 初始化db日志
function Log:start()
    self.db_logger = Mysql()
    self.db_logger:start(g_setting.mysql_ip, g_setting.mysql_port,
                         g_setting.mysql_user, g_setting.mysql_pwd,
                         g_setting.mysql_db, function()
            g_app:one_initialized("db_logger", 1)
    end)
end

-- 关闭文件日志线程及数据库日志线程
function Log.stop()
    -- self.async_file:stop()
    if this.db_logger then this.db_logger:stop() end
end

-- 自定义写文件
function Log.raw_file_printf(path, ...)
    return async_file:append_log_file(path, string.format(...))
end

return Log
