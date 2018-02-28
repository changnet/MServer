-- 各模块加载入口
-- global对象的创建都放在这里，方便热更

E = require_ex "modules.system.error"

g_conn_mgr    = require "network.conn_mgr"
g_timer_mgr   = require "timer.timer_mgr"
g_command_pre = require "command.command_pre"
g_rpc         = require "rpc.rpc"
g_network_mgr = require "network.network_mgr"
g_command_mgr = require "command.command_mgr"

local Srv_conn    = require "network.srv_conn"

-- 仅在gateway使用
if "gateway" == Main.srvname then
    g_setting     = require "gateway.setting"
    g_httpd       = require "http.httpd"
    g_account_mgr = require "modules.account.account_mgr"

    local Clt_conn    = require "network.clt_conn"

    -- db对象的创建，有点特殊。它是全局对象，但不是单例。可以根据业务创建多个db提高效率
    -- 不要多次调用new创建多个对象
    g_mongodb_mgr = require "mongodb.mongodb_mgr"
    if not g_mongodb then g_mongodb = g_mongodb_mgr:new() end
end

-- 仅在world使用
if "world" == Main.srvname then
    g_setting     = require "world.setting"
    g_player_mgr  = require "modules.player.player_mgr"

    g_mongodb_mgr = require "mongodb.mongodb_mgr"
    if not g_mongodb then g_mongodb = g_mongodb_mgr:new() end
end

-- 加载指令注册入口(用require_ex保证热更)
-- 需要在global对象创建后才加载，因为部分模块依赖global对象
require_ex "command/command_header"
