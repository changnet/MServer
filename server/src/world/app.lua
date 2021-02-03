-- app.lua
-- 2018-04-04
-- xzc

-- world进程app

local SrvApp = require "application.srv_app"

local App = oo.class( ..., SrvApp )

-- 初始化
function App:__init( ... )
    SrvApp.__init( self,... )

    self:set_initialize( "area", nil, nil,2 ) -- 等待2个area服连接OK
    self:set_initialize( "gateway" ) -- 等待一个gateway服连接OK
    self:set_initialize( "db_logger",self.db_logger_initialize ) -- db日志
    self:set_initialize( "db_conn",self.db_initialize ) -- 等待连接db
    self:set_initialize( "sys_mail", self.sys_mail_initialize, "db_conn" ) -- 加载全服邮件
end

-- 重写初始化入口
function App:initialize()
    self:module_initialize()

    if not g_network_mgr:srv_listen(g_app_setting.sip, g_app_setting.sport) then
        ERROR( "world server listen fail,exit" )
        os.exit( 1 )
    end

    g_network_mgr:connect_srv( g_app_setting.servers )

    SrvApp.initialize( self )
end

-- 重写关服接口
function App:shutdown()
    g_mongodb:stop() -- 关闭所有数据库链接

    SrvApp.shutdown( self )
end

-- 加载全服邮件
function App:sys_mail_initialize()
    g_mail_mgr:db_load()
end

return App
