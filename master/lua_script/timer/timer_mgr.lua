-- timer_mgr.lua
-- 2017-05-26
-- xzc

-- 定时器管理

local LIMIT = require "global.limits"

local Timer = require "Timer"
local Timer_mgr = oo.singleton( nil,... )

function Timer_mgr:__init()
    self.next_id = 0

    -- 使用weak table防止owner对象不释放内存
    self.owner = {}
    setmetatable( self.owner, {["__mode"]='v'} )

    self.timer = {}
end

function Timer_mgr:get_next_id()
    repeat
        if self.next_id >= LIMIT.INT32_MAX then self.next_id = 0 end

        self.next_id = self.next_id + 1
    until nil == self.timer[self.next_id]

    assert( nil == self.owner[self.next_id] )
    return self.next_id
end

function Timer_mgr:new_timer( owner,after,interval )
    local timer_id = self:get_next_id()

    local timer = Timer( timer_id )
    timer:set( after,interval )

    self.owner[timer_id] = owner
    self.timer[timer_id] = timer

    return timer_id
end

function Timer_mgr:start_timer( timer_id )
    self.timer[timer_id]:start()
end

function Timer_mgr:stop_timer ( timer_id )
    self.timer[timer_id]:stop()
end

function Timer_mgr:del_timer( timer_id )
    self.timer[timer_id]:stop()

    self.owner[timer_id] = nil
    self.timer[timer_id] = nil
end

local timer_mgr = Timer_mgr()

--[[
    为了解决函数引用造成的热更问题，这里统一调用do_timer。如果某个对象拥有多个定时器，
    可通过timer_id来区分
]]
function timer_event( timer_id )
    timer_mgr.owner[timer_id]:do_timer( timer_id )
end

return timer_mgr
