-- 登录、退出

local Loginout = oo.class( nil,... )

-- 检查是否执行登录
function Loginout:check_and_login(ai,entity,param)
    if ai.is_login then return false end -- 已经线
    if ai.logout_time + param.logout_time > ev:now() then return false end

    -- 登录服务器
    local conn_id =
            network_mgr:connect( "127.0.0.1",10002,network_mgr.CNT_CSCN )

    entity:set_conn(conn_id)
    return true
end

function Loginout:on_conn_new(ai,entity)
end

function Loginout:on_handshake(ai,entity)
end

-- 是否执行退出
function Loginout:check_and_logout(ai,entity,param)
end

g_android_cmd:conn_new_register(Loginout.on_conn_new)
g_android_cmd:handshake_register(Loginout.on_handshake)

return Loginout
