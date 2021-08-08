-- misc_cmd.lua
-- 2018-05-13
-- xzc
-- misc系统指令入口
local role_welcome = require "modules.misc.role_welcome"

-- 事件回调
local function role_welcome_cb(func)
    return function(player, ...)
        return func(role_welcome, player, ...)
    end
end

-- 协议回调
local function role_welcome_cmd_cb(cmd, func)
    assert(func, "role_welcome cmd no callback")
    local cb = function(srv_conn, pid, pkt)
        local player = PlayerMgr.get_player(pid)
        if not player then
            elog("role_welcome_cb no player:%d", cmd)
            return
        end

        return func(role_welcome, player, pkt)
    end

    Cmd.reg(cmd, cb)
end

PE.reg(PE_ENTER, role_welcome_cb(role_welcome.on_enter))
role_welcome_cmd_cb(MISC.WELCOME_GET, role_welcome.handle_award)
