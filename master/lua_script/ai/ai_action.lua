-- 通用的ai动作

local Ai_action = {}

function Ai_action.ping( entity )
    entity.ping_ts = entity.ping_ts + 1
    entity:send_pkt( CS.PLAYER_PING,
        {
            x = entity.pid,
            y = entity.ping_ts,
            z = entity.ping_idx + 1,
            way = ev:time(),
            target = { 1,2,3,4,5,6,7,8,9 },
            say = "android ping test android ping test android ping test android ping test android ping test"
        } )
end

return Ai_action