-- 系统事件总线

require "modules.event.event_header"
local System_event = oo.singleton( nil,... )

function System_event:__init()
end

function System_event:fire_event( event_id,... )
end

local sys_ev = System_event()

return sys_ev
