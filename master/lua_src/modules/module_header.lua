-- 各模块加载入口
-- global对象的创建都放在这里，方便热更

-- 非global对象不要放这里，应该由对应模块的入口文件xx_cmd引用。

-- 引用一起基础文件。其他逻辑初始化时可能会用到这些库
require "global.global"
require "global.table"
require "global.string"
require "global.time"

require "modules.system.define"
require "modules.system.hot_fix"
E = require "modules.system.error"

g_log_mgr     = require "modules.log.log_mgr"
g_unique_id   = require "modules.system.unique_id"
g_conn_mgr    = require "network.conn_mgr"
g_timer_mgr   = require "timer.timer_mgr"
g_rpc         = require "rpc.rpc"
g_network_mgr = require "network.network_mgr"
g_command_mgr = require "modules.command.command_mgr"
g_res         = require "modules.res.res"
g_gm          = require "modules.system.gm"

-- 仅在gateway使用
if "gateway" == g_app.srvname then
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
if "world" == g_app.srvname then
    g_setting     = require "world.setting"
    g_player_mgr  = require "modules.player.player_mgr"

    g_mongodb_mgr = require "mongodb.mongodb_mgr"
    if not g_mongodb then g_mongodb = g_mongodb_mgr:new() end
end

-- =============================================================================
-- =============================================================================
-- =============================================================================
-- ====================================指令注册入口==============================
-- =============================================================================
-- =============================================================================

-- 加载指令注册入口
-- 需要在global对象创建后才加载，因为部分模块依赖global对象
-- 公用
require "modules.system.system_cmd" -- 系统模块
require "modules.player.player_cmd" -- 玩家基础模块

-- 仅在gateway使用
if "gateway" == g_app.srvname then
end

-- 仅在world使用
if "world" == g_app.srvname then
    require "modules.chat.chat_cmd" -- 聊天
end
