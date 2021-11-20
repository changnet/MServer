-- app.lua
-- xzc
-- 2018-11-20

-- 场景服入口
local SrvApp = require "application.srv_app"

local App = oo.class(..., SrvApp)

-- 初始化
function App:__init(...)
    SrvApp.__init(self, ...)
end

-- 重写初始化入口
function App:initialize()
    self:module_initialize() -- 加载子模块

    SrvApp.initialize(self)
end

-- 重写初始化结束入口
function App:final_initialize()
    ev:set_app_ev(200) -- 200毫秒回调一次主循环

    SrvApp.final_initialize(self)
end

-- 主事件循环
function App:ev(ms_now)
    EntityMgr.routine(ms_now)
end

return App
