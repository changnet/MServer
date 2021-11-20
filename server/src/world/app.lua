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

    if not SrvMgr.srv_listen(g_app_setting.sip, g_app_setting.sport) then
        elog("world server listen fail,exit")
        return false
    end

    SrvApp.initialize(self)
end

-- 重写关服接口
function App:shutdown()
    g_mongodb:stop() -- 关闭所有数据库链接
    g_log_mgr:close()

    SrvApp.shutdown(self)
end

return App
