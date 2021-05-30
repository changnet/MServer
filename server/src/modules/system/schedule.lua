-- schedule.lua
-- 2018-02-11
-- xzc
-- 系统级定时器，处理一些定时开启、天循环、周循环开启的事件
local Schedule = oo.singleton(...)

function Schedule:__init()
end

-- 定时器回调
function Schedule:do_timer()
end

-- 5秒定时器
function Schedule:register_5s_timer()
end

local schedule = Schedule()

return schedule
