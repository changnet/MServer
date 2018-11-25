-- app.lua
-- xzc
-- 2018-11-20

-- 场景服入口


local Application = require "application.application"

local App = oo.class( Application,... )

-- 初始化
function App:__init( ... )
    Application.__init( self,... )

    self:set_initialize( "world" ) -- 等待一个world服连接OK
    self:set_initialize( "gateway" ) -- 等待一个world服连接OK
    -- self:set_initialize( "dungeon",nil,self.dungeon_initialize ) -- 加载副本数据
end

-- 重写初始化入口
function App:initialize()
    self:module_initialize() -- 加载子模块

    -- 主动连接到gateway和world
    g_network_mgr:connect_srv( g_setting.servers )

    Application.initialize( self )
end

-- 重写初始化结束入口
function App:final_initialize()
    ev:set_app_ev( 200 ) -- 200毫秒回调一次主循环

    Application.final_initialize( self )
end

-- 重写关服接口
function App:shutdown()
    Application.shutdown( self )
end

-- 加载全服邮件
function App:dungeon_initialize()
    
end

-- 主事件循环，设置了ev:set_app_ev后由C++回调
function application_ev( ms_now )
    PLOG("application_ev",ms_now,ev:time())
end

return App
