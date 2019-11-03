-- 系统事件总线

require "modules.event.event_header"
local SystemEvent = oo.singleton( ... )

function SystemEvent:__init()
end

function SystemEvent:fire_event( event_id,... )
end

local sys_ev = SystemEvent()

return sys_ev
