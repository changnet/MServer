-- 通用的ai动作

local Ai_action = {}

function Ai_action.ping( entity )
    entity:send_pkt( CS.PLAYER_PING,{ x = 1,y = 2,z = 3,way = 98786} )
    entity.ping_ts = entity.ping_ts + 1
end

return Ai_action