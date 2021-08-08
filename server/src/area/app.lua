-- app.lua
-- xzc
-- 2018-11-20
-- 场景服入口
local SrvApp = require "application.srv_app"

local App = oo.class(..., SrvApp)

local g_entity_mgr = nil

-- 初始化
function App:__init(...)
    SrvApp.__init(self, ...)

    self:set_initialize("world") -- 等待一个world服连接OK
    self:set_initialize("gateway") -- 等待一个world服连接OK
    self:set_initialize("dungeon", self.dungeon_initialize) -- 加载副本数据
end

-- 重写初始化入口
function App:initialize()
    self:module_initialize() -- 加载子模块

    g_entity_mgr = _G.g_entity_mgr

    -- 主动连接到gateway和world
    SrvMgr.connect_srv(g_app_setting.servers)

    SrvApp.initialize(self)
end

-- 重写初始化结束入口
function App:final_initialize()
    ev:set_app_ev(200) -- 200毫秒回调一次主循环

    SrvApp.final_initialize(self)
end

-- 加载全服邮件
function App:dungeon_initialize()
    g_dungeon_mgr:init_static_dungeon()
end

-- 主事件循环
function App:ev(ms_now)
    g_entity_mgr:routine(ms_now)
end

return App
