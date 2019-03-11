-- 登录、退出

local Loginout = oo.class( nil,... )

-- 检查是否执行登录
function Loginout.check_login(et,param)
    if et.is_login then return false end -- 已经线
    if et.logout_time + param.logout_time > ev:now() then return false end

    return true
end



return Loginout
