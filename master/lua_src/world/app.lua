-- app.lua
-- 2018-04-04
-- xzc

-- world进程app

local Application = require "application.application"

local App = oo.class( Application,... )

-- 初始化
function App:__init( ... )
    Application.__init( self,... )

    self:set_initialize( "area" ) -- 等待一个area服连接OK
    self:set_initialize( "gateway" ) -- 等待一个gateway服连接OK
    self:set_initialize( "db_logger",nil,self.db_logger_initialize ) -- db日志
    self:set_initialize( "db_conn",nil,self.db_initialize ) -- 等待连接db
    self:set_initialize( "sys_mail","db_conn",self.sys_mail_initialize ) -- 加载全服邮件
end

-- 重写初始化入口
function App:initialize()
    self:module_initialize()

    if not g_network_mgr:srv_listen( g_setting.sip,g_setting.sport ) then
        ERROR( "world server listen fail,exit" )
        os.exit( 1 )
    end

    g_network_mgr:connect_srv( g_setting.servers )

    Application.initialize( self )
end

-- 重写关服接口
function App:shutdown()
    g_mongodb_mgr:stop() -- 关闭所有数据库链接

    Application.shutdown( self )
end

-- 加载全服邮件
function App:sys_mail_initialize()
    g_mail_mgr:db_load()
end

return App
