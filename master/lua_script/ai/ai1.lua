-- benchmark测试ai逻辑

local PING_MAX = 10000
local Ai_action = require "ai.ai_action"

local Ai = {}

function Ai.__init( entity,conf )
    entity.ping_ts = 0
    entity.ping_idx = -1 -- 等待进入游戏
end

function Ai.execute( entity )
    -- if not entity.connected then
    --     return Ai_action.connect_server( entity )
    -- end

    if entity.ping_idx < 0 or entity.ping_ts >= PING_MAX then return end

    Ai_action.ping( entity )
end

function Ai.on_enter_world( entity )
    entity.ping_idx = 0
end

function Ai.on_ping( entity )
    
end

g_player_ev:register( PLAYER_EV_ENTER,Ai.on_enter_world )

return Ai