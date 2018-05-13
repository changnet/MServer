-- misc_cmd.lua
-- 2018-05-13
-- xzc

-- misc系统指令入口

local role_welcome = require "modules.misc.role_welcome"

local function role_welcome_cb( func )
    return function ( ... )
        return func( role_welcome,player,... )
    end
end

g_player_ev:register( PLAYER_EV.ENTER,role_welcome.on_enter )
