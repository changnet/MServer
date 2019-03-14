-- 登录、退出

local AST = require "modules.ai.ai_header"

local Loginout = oo.class( nil,... )

-- 检查是否执行登录
function Loginout:check_and_login(ai,entity,param)
    if ai.is_login then return false end -- 已经线
    if ai.logout_time + param.logout_time > ev:now() then return false end

    -- 登录服务器
    local conn_id =
            network_mgr:connect( "127.0.0.1",10002,network_mgr.CNT_CSCN )

    ai.state = AST.LOGIN
    entity:set_conn(conn_id)
    return true
end

function Loginout:on_conn_new(ai,entity)
    android_mgr.conn[conn_id] = nil
    android_mgr.android[android.index] = nil

    ELOG( "android(%d) connect fail:%s",
        android.index,util.what_error(ecode) )
end

function Loginout:on_handshake(ai,entity)
    print( "clt handshake",sec_websocket_accept)
    local android = android_mgr.conn[conn_id]
    android:on_connect()
end

-- 是否执行退出
function Loginout:check_and_logout(ai,entity,param)
end

local function net_cb( cb )
    return function(conn_id)
        return cb()
    end
end

g_android_cmd:conn_new_register(net_cb(Loginout.on_conn_new))
g_android_cmd:handshake_register(net_cb(Loginout.on_handshake))

return Loginout
