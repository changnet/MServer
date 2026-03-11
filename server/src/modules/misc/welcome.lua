-- role_welcome.lua
-- 2018-05-13
-- xzc
-- 新角色欢迎功能，发一封邮件

local SKEY = "welcome"
local WelcomConf = require "config.player.welcome_conf"

-- 发送数据
local function send_info(player, st)
    return player:send_pkt(MISC.WELCOME, {status = st})
end

-- 登录事件
local function on_enter(player)
    local var = Player.get_storage(player, SKEY)
    if var.welcome then return end -- 已经领取过

    return send_info(player, 0)
end

-- 处理前端领取奖励
local function c_get_award(player, pkt)
    local var = Player.get_storage(player, SKEY)
    if var.welcome then return end -- 已经领取过

    var.welcome = 1
    Res.add(player, WelcomConf.resources, LOG.WELCOME)

    -- TODO: 还要发封邮件

    send_info(player, 1)
end

PE.reg(PE_LOGIN, on_enter)
NetMsg.reg(MISC.WELCOME_GET, c_get_award)
