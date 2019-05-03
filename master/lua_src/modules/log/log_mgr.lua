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
    -- fl_logger:start( 3,0 ); -- 暂不启用另外的线程

    self.fl_logger = fl_logger
end

-- 初始化db日志
function Log_mgr:db_logger_init()
    local callback = function()
        self:on_db_logger_init()
    end

    self.db_logger = g_mysql_mgr:new()
    self.db_logger:start( g_setting.mysql_ip,g_setting.mysql_port,
    g_setting.mysql_user,g_setting.mysql_pwd,g_setting.mysql_db,callback )
end

-- db日志初始化完成
function Log_mgr:on_db_logger_init()
    g_app:one_initialized( "db_logger",1 )
end

-- 关闭文件日志线程及数据库日志线程
function Log_mgr:close()
    -- self.fl_logger:stop()
    -- db则由g_mysql_mgr去管理
end

-- 记录登录、退出日志
local login_out_stmt = "INSERT INTO `login_logout` (pid,op_type,op_time) values (%d,%d,%d)"
function Log_mgr:login_or_logout( pid,op_type )
    self.db_logger:insert( string.format(login_out_stmt,pid,op_type,ev:time()) )
end

-- 元宝操作日志
-- 日志里不应该有特殊字符，故ext不做特殊处理
function Log_mgr:gold_log( pid,op_val,new_val,op_type,ext )
    local stmt = string.format(
        "INSERT INTO `gold` (pid,op_val,new_val,op_type,op_time,ext) values (%d,%d,%d,%d,%d,\"%s\")",
        pid,op_val,new_val,op_type,ev:time(),tostring(ext or "") )
    self.db_logger:insert( stmt )
end

-- 记录添加邮件操作日志
-- @who:玩家pid，如果是系统邮件，则为sys
function Log_mgr:add_mail_log( who,mail )
    local ctx = string.format("add_mail:%s,%s",tostring(who),table.dump(mail))
    self.fl_logger:write( "log/mail_log",ctx)
end

-- 记录删除邮件操作日志
-- @who:玩家pid，如果是系统邮件，则为sys
function Log_mgr:del_mail_log( who,mail )
    local ctx = string.format("del_mail:%s,%s",tostring(who),table.dump(mail))
    self.fl_logger:write( "log/mail_log",ctx)
end

local log_mgr = Log_mgr()
return log_mgr
