-- role_welcome.lua
-- 2018-05-13
-- xzc

-- 角色欢迎

local Role_welcome = oo.singleton( nil,... )

-- 登录事件
function Role_welcome:on_enter( player )
    local var = player:get_misc_var()
    if var.welcome then return end -- 已经领取过

    return self:send_info( player )
end

-- 发送数据
function Role_welcome:send_info( player )
end

-- 处理前端领取奖励
function Role_welcome:handle_award()
end

local rw = Role_welcome()

return rw
