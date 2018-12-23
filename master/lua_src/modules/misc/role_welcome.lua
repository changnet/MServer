-- role_welcome.lua
-- 2018-05-13
-- xzc

-- 角色欢迎

local welcom_conf = require_conf("player_welcome")

local Role_welcome = oo.singleton( nil,... )

-- 登录事件
function Role_welcome:on_enter( player )
    local var = player:get_misc_root()
    if var.welcome then return end -- 已经领取过

    return self:send_info( player,0 )
end

-- 发送数据
function Role_welcome:send_info( player,st )
    return player:send_pkt( SC.MISC_WELCOME,{status = st})
end

-- 处理前端领取奖励
function Role_welcome:handle_award( player,pkt )
    local var = player:get_misc_root()
    if var.welcome then return end -- 已经领取过

    var.welcome = 1
    g_res:add_res( player,welcom_conf.resources,LOG.WELCOME )

    -- TODO: 还要发封邮件

    self:send_info( player,1 )
end

local rw = Role_welcome()

return rw
