-- app.lua
-- 2018-04-04
-- xzc
-- 网关进程app
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
        elog("gateway server listen fail,exit")
        return false
    end

    if not g_httpd:start(g_app_setting.hip, g_app_setting.hport) then
        elog("gateway http listen fail,exit")
        return false
    end

    SrvApp.initialize(self)
end

-- 重写初始化结束入口
function App:final_initialize()
    if not CltMgr.clt_listen(g_app_setting.cip, g_app_setting.cport) then
        elog("gateway client listen fail,exit")
        return false
    end

    SrvApp.final_initialize(self)
end

-- 重写关服接口
function App:shutdown()
    g_mongodb:stop() -- 关闭所有数据库链接
    g_log_mgr:close()

    SrvApp.shutdown(self)
end

return App
