-- benchmark测试ai逻辑

local PING_MAX = 1000
local Ai_action = require "ai.ai_action"

local Ai = {}

local ping_cnt = 0
local ping_finish = 0
local ping_start  = 0

function Ai.__init( entity,conf )
    entity.ping_ts = 0
    entity.ping_idx = -1 -- 等待进入游戏

    ping_cnt = ping_cnt + 1
end

function Ai.execute( entity )
    if entity.ping_idx < 0 or entity.ping_ts >= PING_MAX then return end

    if 0 == ping_start then
        PLOG( "andrid ping start" )
        ping_start = ev:time()
    end

    Ai_action.ping( entity )
    Ai_action.ping( entity )
    Ai_action.ping( entity )
    Ai_action.ping( entity )
end

function Ai.on_enter_world( entity )
    entity.ping_idx = 0
end

function Ai.on_ping( entity )
    entity.ping_idx = entity.ping_idx + 1

    if entity.ping_idx >= PING_MAX then ping_finish = ping_finish + 1 end
    if ping_finish == ping_cnt then
        PLOG( "android finish ping,time %d",ev:time() - ping_start )
    end
end

g_player_ev:register( PLAYER_EV_ENTER,Ai.on_enter_world )

return Ai