-- benchmark测试ai逻辑

local PING_MAX = 1000
local PING_ONE_TIME = 2
assert( 0 == PING_MAX%PING_ONE_TIME )

local Ai_action = require "ai.ai_action"

local Ai = {}

local ping_cnt = 0
local ping_idx = 0
local ping_finish = 0
local ping_start  = 0

local finish_entity = {}

function Ai.__init( entity,conf )
    entity.ping_ts = 0
    entity.ping_idx = -1 -- 等待进入游戏

    ping_cnt = ping_cnt + 1
end

function Ai.execute( entity )
    if entity.ping_idx < 0 or entity.ping_ts >= PING_MAX then return end

    if 0 == ping_start then
        PLOG( "android ping start" )
        ping_start = ev:time()
    end

    for index = 1,PING_ONE_TIME do Ai_action.ping( entity ) end
end

function Ai.on_enter_world( entity )
    entity.ping_idx = 0
end

function Ai.on_ping( entity,pkt )
    entity.ping_idx = entity.ping_idx + 1

    if pkt.y ~= entity.ping_idx then
        PLOG( "pkt pid not match,android %d,expect %d,got %d,ping %d",
                     entity.index,entity.ping_idx,pkt.y,ping_idx )
        os.exit(1)
        return
    end

    ping_idx = ping_idx + 1
    if entity.ping_idx >= PING_MAX then
        ping_finish = ping_finish + 1
        if finish_entity[entity] ~= nil then
            PLOG( "dumplicate finish,android %d,send %d,recv %d,total %d",
                    entity.index,entity.ping_ts,entity.ping_idx,ping_idx )
            os.exit(1)
            return
        end
        finish_entity[entity] = 1
    end
    if ping_finish == ping_cnt then
        PLOG( "android finish ping,%d entity recv %d package,time %d",
                            ping_cnt,ping_idx,ev:time() - ping_start )
    end

    if ping_idx > PING_MAX*ping_cnt then
        PLOG( "too many android ping package receive" )
    end
end

g_player_ev:register( PLAYER_EV_ENTER,Ai.on_enter_world )

return Ai