-- 各模块加载入口

-- 所有进程都是从这里开始加载所需要的模块的
-- 该进程只加载该进程需要的逻辑文件，以提高加载速度，减少内存占用
-- 但经验表明哪些文件是该进程所需要是不好判断的，各个模块之间的require一层套一层
-- 请务必保证大模块不被滥用即可(比如场景进程的怪物配置，可达几十M大小，不要在其他进程引用)
-- 其他小模块即使引用了也不会有太大的影响

-- 不需要引用相关的文件，可用xxx_cmd入口文件隔离
-- 例如gateway需要通过rpc调用场景的一个函数，但场景引用了怪物配置，这时gateway进程就
-- 不能直接引用场景的文件，而是这两个进程同时引用 entity_cmd，然后gateway调用
-- 场景的entity_cmd的函数，再通过全局对象来对场景进行操作

local APP_TYPE = APP_TYPE

-- 根据服务器app类型判断是否加载该文件
local function require_app(path, app_type, ...)
    if not app_type then return end

    if APP_TYPE == app_type then return require(path) end

    return require_app(path, ...)
end


-- 引用一起基础文件。其他逻辑初始化时可能会用到这些库
require "global.global"
require "global.table"
require "global.math"
require "global.string"
require "global.time"
require "global.name"

require_define "modules.system.define"

require "modules.system.hot_fix"
E = require "modules.system.error"


-- 加载服务器配置
-- 部分服务器配置不支持热更，很多字段，比如端口id热更了，也不可能改变
g_setting = require "setting.setting" -- require_no_update
g_app_setting = g_setting[g_app.name]
assert(g_app_setting, "no server conf found")

g_stat_mgr = require "statistic.statistic_mgr"
g_log_mgr = require "modules.log.log_mgr"
g_unique_id = require "modules.system.unique_id"
g_timer_mgr = require "timer.timer_mgr"
require "rpc.rpc"
require "network.cmd"
require "network.srv_mgr"
g_res = require "modules.res.res"
require "modules.system.gm"
require "modules.event.player_event"
require "modules.event.system_event"
g_lang = require "modules.lang.lang"
g_mail_mgr = require "modules.mail.mail_mgr"

require "modules.system.ping"

local Mongodb = require "mongodb.mongodb"

g_httpd = require_app("http.httpd", GATEWAY)
g_mongodb = g_mongodb
g_map_mgr = nil
g_dungeon_mgr = nil

require_app("network.clt_mgr", GATEWAY, WORLD)
require_app("modules.account.account_mgr", GATEWAY)

require_app("modules.player.player_mgr", WORLD)

g_map_mgr = require_app("modules.dungeon.map_mgr", AREA)
require_app("modules.entity.entity_mgr", AREA)
g_dungeon_mgr = require_app("modules.dungeon.dungeon_mgr", AREA)

-- 仅在gateway使用
if GATEWAY == APP_TYPE then
    if not g_mongodb then g_mongodb = Mongodb() end
end
-- 仅在world使用
if WORLD == APP_TYPE then
    if not g_mongodb then g_mongodb = Mongodb() end
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
require "modules.mail.mail_cmd" -- 邮件模块

require_app("modules.chat.chat", WORLD) -- 聊天
require_app("modules.bag.bag_cmd", WORLD) -- 背包
require_app("modules.misc.misc_cmd", WORLD) -- 杂七杂八的小功能

require_app("modules.entity.entity_cmd", AREA, WORLD) -- 实体相关
require_app("modules.dungeon.test_dungeon", AREA) -- 测试不同场景进程中切换

-- =============================================================================
-- =============================================================================

-- 生成函数及其对应的名字
make_name()

-- 生成回调
Cmd.make_this_cb()
