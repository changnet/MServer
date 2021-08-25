-- role_welcome.lua
-- 2018-05-13
-- xzc
-- 角色欢迎


local WelcomConf = require("config.player_welcome")

-- 发送数据
local function send_info(player, st)
    return player:send_pkt(MISC.WELCOME, {status = st})
end

-- 登录事件
local function on_enter(player)
    local var = player:storage()
    if var.welcome then return end -- 已经领取过

    return send_info(player, 0)
end

-- 处理前端领取奖励
local function handle_award(player, pkt)
    local var = player:storage()
    if var.welcome then return end -- 已经领取过

    var.welcome = 1
    g_res:add_res(player, WelcomConf.resources, LOG.WELCOME)

    -- TODO: 还要发封邮件

    send_info(player, 1)
end

PE.reg(PE_ENTER, on_enter)
Cmd.reg_player(MISC.WELCOME_GET, handle_award)
