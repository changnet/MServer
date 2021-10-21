-- timer_mgr.lua
-- 2017-05-26
-- xzc
-- 定时器管理
local LIMIT = require "global.limits"

local Timer = require "engine.Timer"
local TimerMgr = oo.singleton(...)

local method_thunk = method_thunk

function TimerMgr:__init()
    self.next_id = 0

    self.timer = {}
end

function TimerMgr:get_next_id()
    repeat
        if self.next_id >= LIMIT.INT32_MAX then self.next_id = 0 end

        self.next_id = self.next_id + 1
    until nil == self.timer[self.next_id]

    return self.next_id
end

-- 创建按间隔循环的定时器
-- @param after 延迟N毫秒启动定时器
-- @param sec 循环间隔，单位毫秒，0表示不循环
-- @param times 循环次数，-1表示永久
-- @param this 回调对象，没有可以不传此参数
-- @param method 回调函数
-- @param ... 其他回调参数
-- @return 定时器id
function TimerMgr:interval(after, sec, times, this, method, ...)
    assert(sec >= 0, "repeat interval MUST > 0")

    local timer_id = self:get_next_id()

    local timer = Timer(timer_id)
    timer:set(after, sec)

    local cb
    if "function" == type(this) then
        cb = func_thunk(this, method, ...)
    else
        cb = method_thunk(this, method, ...)
    end
    local info = {cb = cb, timer = timer}

    if times >= 0 then
        info.ts = 0
        info.times = times
    end
    self.timer[timer_id] = info

    timer:start()
    return timer_id
end

-- 创建一次延时回调
-- @param after 延迟N毫秒启动定时器
-- @param this 回调对象，没有可以不传此参数
-- @param method 回调函数
-- @param ... 其他回调参数
-- @return 定时器id
function TimerMgr:timeout(after, this, method, ...)
    return self:interval(after, 0, 1, this, method, ...)
end

-- 停止定时器
function TimerMgr:stop(timer_id)
    local info = self.timer[timer_id]
    if not info then return end

    info.timer:stop()

    self.timer[timer_id] = nil
end

local timer_mgr = TimerMgr()

--[[
    C++ 统一回调这个函数，根据timer_id区分
]]
function timer_event(timer_id)
    local timer = timer_mgr.timer[timer_id]
    if not timer then
        elog("timer no cb found,abort %d", timer_id)
        return timer_mgr:stop(timer_id)
    end

    if timer.times then
        timer.ts = timer.ts + 1
        if timer.ts >= timer.times then timer_mgr:stop(timer_id) end
    end

    return timer.cb()
end

return timer_mgr
