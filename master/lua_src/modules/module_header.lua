-- 各模块加载入口
-- global对象的创建都放在这里，方便热更

-- 非global对象不要放这里，应该由对应模块的入口文件xx_cmd引用。

-- 引用一起基础文件。其他逻辑初始化时可能会用到这些库
require "global.global"
require "global.table"
require "global.string"
require "global.time"
require "global.name"
require "global.require_conf"

require "modules.system.define"
require "modules.system.hot_fix"
E = require "modules.system.error"

-- 加载服务器配置
-- 部分服务器配置不支持热更，很多字段，比如端口id你热更了，也不可能改变
g_setting = require "setting.setting" -- no_update_require
g_app_setting = g_setting[g_app.srvname]
assert( g_app_setting,"no server conf found" )

-- !!! 放到这里的，一定要是singleton，不然会被销毁重建
-- TODO:找个方法检测一下是否singleton

g_log_mgr     = require "modules.log.log_mgr"
g_unique_id   = require "modules.system.unique_id"
g_conn_mgr    = require "network.conn_mgr"
g_timer_mgr   = require "timer.timer_mgr"
g_rpc         = require "rpc.rpc"
g_authorize   = require "modules.system.authorize"
g_network_mgr = require "network.network_mgr"
g_command_mgr = require "modules.command.command_mgr"
g_res         = require "modules.res.res"
g_gm          = require "modules.system.gm"
g_mysql_mgr   = require "mysql.mysql_mgr"
g_player_ev   = require "modules.event.player_event"
g_system_ev   = require "modules.event.system_event"
g_lang        = require "modules.lang.lang"
g_mail_mgr    = require "modules.mail.mail_mgr"

-- 仅在gateway使用
if "gateway" == g_app.srvname then
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
    g_player_mgr  = require "modules.player.player_mgr"

    g_mongodb_mgr = require "mongodb.mongodb_mgr"
    if not g_mongodb then g_mongodb = g_mongodb_mgr:new() end
end

-- 仅在area使用
if "area" == g_app.srvname then
    g_map_mgr = require "modules.dungeon.map_mgr"
    g_entity_mgr = require "modules.entity.entity_mgr"
    g_dungeon_mgr = require "modules.dungeon.dungeon_mgr"
end

-- =============================================================================
-- =============================================================================
-- =============================================================================
-- ====================================指令注册入口===============================
-- =============================================================================
-- =============================================================================

-- 加载指令注册入口
-- 需要在global对象创建后才加载，因为部分模块依赖global对象

-- 公用
require "modules.system.system_cmd" -- 系统模块
require "modules.player.player_cmd" -- 玩家基础模块
require "modules.mail.mail_cmd"     -- 邮件模块

-- 仅在gateway使用
if "gateway" == g_app.srvname then
end

-- 仅在world使用
if "world" == g_app.srvname then
    require "modules.chat.chat_cmd" -- 聊天
    require "modules.bag.bag_cmd"   -- 背包
    require "modules.misc.misc_cmd"     -- 杂七杂八的小功能
end

-- 仅在area使用
if "area" == g_app.srvname then
    require "modules.entity.entity_cmd" -- 实体相关
end
