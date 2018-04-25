-- log_mgr.lua
-- 2018-04-24
-- xzc

-- 日志管理

local Log = require "Log"
require "modules.log.log_header"

local Log_mgr = oo.singleton( nil,... )

-- 初始化
function Log_mgr:__init()
    -- 启动文件日志线程
    local fl_logger = Log()
    fl_logger:start( 3,0 ); -- 3秒写入一次

    self.fl_logger = fl_logger
end

-- 初始化db日志
function Log_mgr:db_logger_init()
end

-- 关闭文件日志线程及数据库日志线程
function Log_mgr:close()
    self.fl_logger:stop()
end

local log_mgr = Log_mgr()
return log_mgr
