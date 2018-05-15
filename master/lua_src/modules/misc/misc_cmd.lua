-- misc_cmd.lua
-- 2018-05-13
-- xzc

-- misc系统指令入口

local role_welcome = require "modules.misc.role_welcome"

-- 事件回调
local function role_welcome_cb( func )
    return function ( player,... )
        return func( role_welcome,player,... )
    end
end

-- 协议回调
local function role_welcome_cmd_cb( cmd,func )
    return function ( srv_conn,pid,pkt )
        local player = g_player_mgr:get_player( pid )
        if not player then
            ELOG( "role_welcome_cb no player:%d",cmd )
            return
        end

        return func( role_welcome,player,pkt )
    end
end

g_player_ev:register( PLAYER_EV.ENTER,role_welcome_cb(role_welcome.on_enter) )
g_command_mgr:clt_register( CS.MISC_WELCOME_GET,
    role_welcome_cmd_cb(role_welcome.handle_award) )


