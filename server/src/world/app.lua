-- app.lua
-- 2018-04-04
-- xzc
-- world进程app
local SrvApp = require "application.srv_app"

local App = oo.class(..., SrvApp)

-- 初始化
function App:__init(...)
    SrvApp.__init(self, ...)
end

-- 重写初始化入口
function App:initialize()
    self:module_initialize()

    SrvApp.initialize(self)
end

-- 重写关服接口
function App:shutdown()
    SrvApp.shutdown(self)
end

return App
