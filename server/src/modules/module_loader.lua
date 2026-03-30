-- 通用模块加载入口
--[[
不同进程、worker的逻辑不一样，需要加载的模块也不一样，很难统一。完全分开又发现会有一些
相同逻辑需要复用，因此定了一些规则

1. process_loader、module_loader是通用的loader。业务相关的一般统一用module_loader

2. 如果某个进程、worker差异比较大，可以自定义一个loader

3. 所有业务模块必须都放loader文件，热更会重载此文件，不放这里可能热更不同
    如果一个模块有多个文件，入口文件放loader，其他文件在模块内引用是没问题的

4. 放loader的模块都是可热更的，不能热更的放startup.xxx_init
]]

local W = W
local require = require_worker


-- 引用一起基础文件。其他逻辑初始化时可能会用到这些库
require "engine.preloader"

table.readonly(EMPTY)

E = require "modules.system.error"

require("protocol.protocol")
require("message.pbc")
require("message.net_msg")

-- P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0
-- 在加载其他业务模块之前优先级p0的逻辑
Pbc.update()
NetMsg.load_forward_msg()
-- P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0

require("network.clt_mgr", W.GATEWAY)
require("account.login", W.GATEWAY)
require("account.account_mgr", W.ACCOUNT)
require("misc.alert")

require("player.player_sync", W.PLAYER, W.GAME, W.SCENE)
require("player.property", W.PLAYER, W.GAME, W.SCENE)
require("event.player_event", W.PLAYER)
require("player.player", W.PLAYER, W.GAME, W.SCENE)
require("player.player_mgr", W.PLAYER, W.GAME, W.SCENE)
require("misc.welcome", W.PLAYER)

-- 邮件模块
require("mail.mail_internal")
require("mail.mail_off", W.GAME)
require("mail.mail_sys", W.GAME)
require("mail.mail_player", W.PLAYER)
require("mail.mail")
