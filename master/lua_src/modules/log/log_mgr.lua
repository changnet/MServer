-- log_mgr.lua
-- 2018-04-24
-- xzc

-- 日志管理

local Log = require "Log"
require "modules.log.log_header"

local LogMgr = oo.singleton( ... )

-- 初始化
function LogMgr:__init()
    -- 启动文件日志线程
    -- local async_file = Log()
    -- async_file:start( 3,0 ); -- 暂不启用另外的线程

    self.async_file = __g_async_log -- async_file
end

-- 初始化db日志
function LogMgr:db_logger_init()
    local callback = function()
        self:on_db_logger_init()
    end

    self.db_logger = g_mysql_mgr:new()
    self.db_logger:start( g_setting.mysql_ip,g_setting.mysql_port,
        g_setting.mysql_user,g_setting.mysql_pwd,g_setting.mysql_db,callback )
end

-- db日志初始化完成
function LogMgr:on_db_logger_init()
    g_app:one_initialized( "db_logger",1 )
end

-- 关闭文件日志线程及数据库日志线程
function LogMgr:close()
    -- self.async_file:stop()
    -- db则由g_mysql_mgr去管理
end

-- 记录登录、退出日志
local login_out_stmt = "INSERT INTO `login_logout` (pid,op_type,op_time) values (%d,%d,%d)"
function LogMgr:login_or_logout( pid,op_type )
    self.db_logger:insert( string.format(login_out_stmt,pid,op_type,ev:time()) )
end

-- 元宝操作日志
-- 日志里不应该有特殊字符，故ext不做特殊处理
function LogMgr:gold_log( pid,op_val,new_val,op_type,ext )
    local stmt = string.format(
        "INSERT INTO `gold` (pid,op_val,new_val,op_type,op_time,ext) values (%d,%d,%d,%d,%d,\"%s\")",
        pid,op_val,new_val,op_type,ev:time(),tostring(ext or "") )
    self.db_logger:insert( stmt )
end

-- 记录添加邮件操作日志
-- @who:玩家pid，如果是系统邮件，则为sys
function LogMgr:add_mail_log( who,mail )
    local ctx = string.format("add_mail:%s,%s",tostring(who),table.dump(mail))
    self.async_file:write( "log/mail_log",ctx)
end

-- 记录删除邮件操作日志
-- @who:玩家pid，如果是系统邮件，则为sys
function LogMgr:del_mail_log( who,mail )
    local ctx = string.format("del_mail:%s,%s",tostring(who),table.dump(mail))
    self.async_file:write( "log/mail_log",ctx )
end

-- 自定义写文件
function LogMgr:raw_file_printf( path,... )
    return self.async_file:write( path,string.format(...) )
end

local log_mgr = LogMgr()
return log_mgr
