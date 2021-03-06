-- app.lua
-- 2018-04-04
-- xzc
-- 网关进程app
local SrvApp = require "application.srv_app"

local App = oo.class(..., SrvApp)

-- 初始化
function App:__init(...)
    SrvApp.__init(self, ...)

    self:set_initialize("area", nil, nil, 2) -- 等待一个area服连接OK
    self:set_initialize("world") -- 等待一个world服OK
    self:set_initialize("db_logger", self.db_logger_initialize) -- db日志
    self:set_initialize("db_conn", self.db_initialize) -- 等待连接db
    -- 等待自增id数据加载
    self:set_initialize("uniqueid_data", self.uniqueid_initialize, "db_conn")
    -- 等待帐号数据加载
    self:set_initialize("acc_data", self.acc_initialize, "db_conn")
end

-- 重写初始化入口
function App:initialize()
    self:module_initialize()

    if not g_srv_mgr:srv_listen(g_app_setting.sip, g_app_setting.sport) then
        ERROR("gateway server listen fail,exit")
        os.exit(1)
    end

    if not g_httpd:start(g_app_setting.hip, g_app_setting.hport) then
        ERROR("gateway http listen fail,exit")
        os.exit(1)
    end

    SrvApp.initialize(self)
end

-- 重写初始化结束入口
function App:final_initialize()
    if not g_clt_mgr:clt_listen(g_app_setting.cip, g_app_setting.cport) then
        ERROR("gateway client listen fail,exit")
        os.exit(1)
    end

    SrvApp.final_initialize(self)
end

-- 重写关服接口
function App:shutdown()
    g_mongodb:stop() -- 关闭所有数据库链接
    g_log_mgr:close()

    SrvApp.shutdown(self)
end

-- 帐号数据初始化
function App:acc_initialize()
    g_account_mgr:db_load()
end

return App
