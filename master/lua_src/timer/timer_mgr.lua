-- timer_mgr.lua
-- 2017-05-26
-- xzc

-- 定时器管理

local LIMIT = require "global.limits"

local Timer = require "Timer"
local TimerMgr = oo.singleton( ... )

local method_thunk = method_thunk

function TimerMgr:__init()
    self.next_id = 0

    -- 使用weak table防止owner对象不释放内存
    self.cb = {}
    -- setmetatable( self.cb, {["__mode"]='v'} )

    self.timer = {}
end

function TimerMgr:get_next_id()
    repeat
        if self.next_id >= LIMIT.INT32_MAX then self.next_id = 0 end

        self.next_id = self.next_id + 1
    until nil == self.timer[self.next_id]

    ASSERT( nil == self.cb[self.next_id],self.next_id )
    return self.next_id
end

-- 创建新定时器
-- @after:延迟N秒启动定时器，可以是小数，精度是毫秒级的
-- @interval:按N秒循环回调
-- @this:回调对象
-- @method:回调函数
function TimerMgr:new_timer( after,interval,this,method,... )
    local timer_id = self:get_next_id()

    local timer = Timer( timer_id )
    timer:set( after,interval )

    self.cb[timer_id] = method_thunk( this,method,... )
    self.timer[timer_id] = timer

    timer:start()
    return timer_id
end

-- 重启定时器
function TimerMgr:start_timer( timer_id )
    self.timer[timer_id]:start()
end

-- 暂停定时器
function TimerMgr:stop_timer ( timer_id )
    self.timer[timer_id]:stop()
end

-- 删除定时器
function TimerMgr:del_timer( timer_id )
    self.timer[timer_id]:stop()

    self.cb[timer_id] = nil
    self.timer[timer_id] = nil
end

local timer_mgr = TimerMgr()

--[[
    C++ 统一回调这个函数，根据timer_id区分
]]
function timer_event( timer_id )
    local cb = timer_mgr.cb[timer_id]
    if not cb then
        ERROR( "timer no cb found,abort %d",timer_id )
        return timer_mgr:del_timer( timer_id )
    end

    return cb()
end

return timer_mgr
