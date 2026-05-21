-- role_welcome.lua
-- 2018-05-13
-- xzc
-- 新角色欢迎功能，发一封邮件

local WelcomConf = require "config.player.welcome_conf"

local function get_storage(player)
    local stg = Player.get_storage(player, "welcome")
    if not stg then
        stg = Player.set_storage(player, "welcome", {})
    end

    return stg
end

-- 发送数据
local function send_info(player, pkt)
    return NetMsg.send(player, M.Welcome, pkt)
end

-- 登录事件
local function on_login(player)
    local stg = get_storage(player)

    return send_info(player, stg)
end

-- 处理前端领取奖励
local function c_get_award(player, pkt)
    local stg = get_storage(player)
    if stg.status then return end -- 已经领取过

    stg.status = 1
    Res.add(player, WelcomConf.resources, LOG.WELCOME)

    -- TODO: 还要发封邮件

    send_info(player, stg)
end

Event.reg(EV.LOGIN, on_login)
NetMsg.reg(M.Welcome, c_get_award)
