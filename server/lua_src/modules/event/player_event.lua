-- 玩家事件总线

require "modules.event.event_header"

local PlayerEvent = oo.singleton( ... )

-- 事件注册放到upvalue，这样热更时才能刷新
local event_map = {}

function PlayerEvent:__init()
end

function PlayerEvent:register( event_id,handler )
    if not event_map[event_id] then event_map[event_id] = {} end

    -- 有可能是热更新时重新注册
    for _,hdl in pairs( event_map[event_id] ) do
        if hdl == handler then return end
    end

    table.insert( event_map[event_id],handler )
end

function PlayerEvent:fire_event( event_id,... )
    if not event_map[event_id] then return end

    for _,hdl in pairs( event_map[event_id] ) do hdl( ... ) end
end

local event = PlayerEvent()

return event