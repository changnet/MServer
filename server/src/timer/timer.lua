-- timer.lua
-- 2017-05-26
-- xzc

-- 定时器
Timer = {}

local LIMIT = require "global.limits"
local TimerBase = require "engine.Timer"

local this = global_storage("Timer", {
    timer = {},
    next_id = 0,
})

local min_timer = {} -- 分钟级定时器回调

local func_thunk = func_thunk
local func_name_thunk = func_name_thunk

-- 分配一个唯一的timer id
local function get_next_id()
    repeat
        if this.next_id >= LIMIT.INT32_MAX then this.next_id = 0 end

        this.next_id = this.next_id + 1
    until nil == this.timer[this.next_id]

    return this.next_id
end

-- 注册一个分钟回调的定时器
function Timer.reg_min_interval(func, ...)
    -- 这种定时器通常用于定时检测，精度要求不高
    -- 假如定时存库，定时删除缓存
    -- 这个定时器会修正为整分的时候触发(即x分0秒)，用于各种活动开关（比如充值活动）
    table.insert(min_timer, func)
end

-- 创建按间隔循环的定时器
-- @param after 延迟N毫秒执行第一次回调
-- @param msec 循环间隔，单位毫秒，0表示不循环
-- @param times 循环次数，-1表示永久
-- @param func 回调函数
-- @param ... 其他回调参数
-- @return 定时器id
function Timer.interval(after, msec, times, func, ...)
    assert(msec >= 0, "repeat interval MUST > 0")

    local timer_id = get_next_id()

    local timer = TimerBase(timer_id)
    timer:set(after, msec)

    local cb
    if after > 10 or (times > 5 or times < 0) then
        -- 如果回调时间很长，则需要该函数能热更
        cb = func_name_thunk(func, ...)
    else
        -- 如果回调时间很短，则在这段时间内需要热更的机率很小
        -- 假如还是刚好遇到热更，则对应的业务逻辑必须自己处理好
        -- 因为即使使用能热更的方式，也没办法控制热更时和定时器回调的谁前谁后时机
        cb = func_thunk(func, ...)
    end

    this.timer[timer_id] = {
        ts = 0, -- 已回调次数
        times = times or 1, -- 总回调次数
        cb = cb,
        timer = timer
    }

    timer:start()
    return timer_id
end

-- 创建一次延时回调
-- @param after N毫秒后回调
-- @param func 回调函数
-- @param ... 其他回调参数
-- @return 定时器id
function Timer.timeout(after, func, ...)
    return Timer.interval(after, 0, 1, func, ...)
end

-- 停止定时器
function Timer.stop(timer_id)
    local info = this.timer[timer_id]
    if not info then return false end

    info.timer:stop()

    this.timer[timer_id] = nil
    return true
end

-- 发起一个utc时间定时器（受调时间影响）
function Timer.periodic()
end

--[[
    C++ 统一回调这个函数，根据timer_id区分
]]
function timer_event(timer_id)
    local timer = this.timer[timer_id]
    if not timer then
        elog("timer no cb found,abort %d", timer_id)
        return Timer.stop(timer_id)
    end

    local times = timer.times
    if times > 0 then -- -1表示无限次数循环
        timer.ts = timer.ts + 1
        if timer.ts >= times then Timer.stop(timer_id) end
    end

    return timer.cb()
end

return Timer
