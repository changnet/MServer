-- app.lua
-- 2018-04-04
-- xzc

-- 网关进程app

local Application = require "application.application"

local App = oo.class( Application,... )

-- 初始化
function App:__init( ... )
    Application.__init( self,... )

    self:set_initialize( "world" ) -- 等待一个world服OK
    self:set_initialize( "db_conn",nil,self.db_initialize ) -- 等待连接db
    -- 等待自增id数据加载
    self:set_initialize( "uniqueid_data","db_conn",self.uniqueid_initialize )
    -- 等待帐号数据加载
    self:set_initialize( "acc_data","db_conn",self.acc_initialize )
end

-- 重写初始化入口
function App:initialize()
    self:module_initialize()

    if not g_network_mgr:srv_listen( g_setting.sip,g_setting.sport ) then
        ELOG( "gateway server listen fail,exit" )
        os.exit( 1 )
    end

    if not g_httpd:http_listen( g_setting.hip,g_setting.hport ) then
        ELOG( "gateway http listen fail,exit" )
        os.exit( 1 )
    end

    Application.initialize( self )
end

-- 重写初始化结束入口
function App:final_initialize()
    if not g_network_mgr:clt_listen( g_setting.cip,g_setting.cport ) then
        ELOG( "gateway client listen fail,exit" )
        os.exit( 1 )
    end

    Application.final_initialize( self )
end

-- 重写关服接口
function App:shutdown()
    g_mongodb_mgr:stop() -- 关闭所有数据库链接
end

-- 帐号数据初始化
function App:acc_initialize()
    g_account_mgr:db_load()
end

return App
