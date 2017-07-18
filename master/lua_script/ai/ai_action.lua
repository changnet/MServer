-- 通用的ai动作

local Ai_Action = {}

function Ai_Action.ping( entity )
    self:send_pkt( CS.PLAYER_PING,{ x = 1,y = 2,z = 3,way = 98786} )
    entity.ping_ts = entity.ping_ts + 1
end