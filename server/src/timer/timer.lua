-- timer.lua
-- 2017-05-26
-- xzc

-- 定时器
Timer = {}

--[[
定时器分为两种定时器：
1. 单调递增定时器(CLOCK_MONOTONIC)
精度为毫秒级，调系统时间不影响这个定时器，对应下面的timer_start接口

2. 实时定时器(REAL_TIME)
采用UTC计时，精度为秒级。调时间会影响这个定时器。对应下面的periodic_start接口
]]

-- 定时器修正规则，这个在C++定义
local P_ALIGN = 1

local LIMIT = require "global.limits"

local this = memory("Timer", {
    timer = {},
    next_id = 0,
})

local min_timer = {} -- 分钟级定时器回调

local make_func_cb = Rtti.make_func_cb
local make_func_name_cb = Rtti.make_func_name_cb

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

-- 挂起当前协程，等待N毫秒后继续
-- @param after 毫秒，0则在下一帧执行
function Timer.wait(after)
    local session = CoPool.current_session()

    local timer_id = get_next_id()
    this.timer[timer_id] = {session = session}

    if 0 == after then
        -- 0比较特殊，纯粹是为了异步执行，避免数据回溯或者调用嵌套。不需要发起定时器
        g_thread:emplace_message(0, 0, ThreadMessage.TIMER, nil, timer_id)
    else
        g_thread:timer_start(timer_id, after, 0, P_ALIGN)
    end

    return coroutine.yield()
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

    local cb
    if after > 5000 or (times < 0 or msec * times > 5000) then
        -- 如果回调时间很长，则需要该函数能热更
        cb = make_func_name_cb(func, ...)
    else
        -- 如果回调时间很短，则在这段时间内需要热更的机率很小
        -- 假如还是刚好遇到热更，则对应的业务逻辑必须自己处理好
        -- 因为即使使用能热更的方式，也没办法控制热更时和定时器回调的谁前谁后时机
        cb = make_func_cb(func, ...)
    end

    this.timer[timer_id] = {
        ts = 0, -- 已回调次数
        times = times or 1, -- 总回调次数
        cb = cb,
    }

    if 0 == after and 1 == times then
        return g_thread:emplace_message(0, 0, ThreadMessage.TIMER, nil, timer_id)
    end
    local e = g_thread:timer_start(timer_id, after, msec, P_ALIGN)
    if e <= 0 then
        this.timer[timer_id] = nil
        eprintf("interval start fail: id = %d, e = %d", timer_id, e)
        return -1
    end
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
    if not info then
        -- timer回调是把事件放到队列，假如这时还来不及处理
        -- 其他逻辑则删除了这个定时器，就会出现找不到的情况，不一定是错误
        printf("timer stop no id found %d: %s", timer_id or 0, debug.traceback())
        return false
    end

    this.timer[timer_id] = nil
    if info.periodic then
        g_thread:periodic_stop(timer_id)
    else
        g_thread:timer_stop(timer_id)
    end

    return true
end

-- 发起一个utc时间定时器（精度为秒，受调时间影响）
-- @param after N秒后第一次回调
-- @param sec 循环间隔，单位秒，0表示不循环
-- @param tims 循环次数，-1表示永久
-- @param func 回调函数
-- @param ... 其他回调参数
-- @return 定时器id
function Timer.periodic(after, sec, times, func, ...)
    assert(sec >= 0, "repeat interval MUST > 0")

    local timer_id = get_next_id()
    local cb
    if after > 5 or (times < 0 or sec * times > 5) then
        -- 如果回调时间很长，则需要该函数能热更
        cb = make_func_name_cb(func, ...)
    else
        -- 如果回调时间很短，则在这段时间内需要热更的机率很小
        -- 假如还是刚好遇到热更，则对应的业务逻辑必须自己处理好
        -- 因为即使使用能热更的方式，也没办法控制热更时和定时器回调的谁前谁后时机
        cb = make_func_cb(func, ...)
    end

    this.timer[timer_id] = {
        ts = 0, -- 已回调次数
        times = times or 1, -- 总回调次数
        cb = cb,
        periodic = true,
    }

    local e = g_thread:periodic_start(timer_id, after, sec, P_ALIGN)
    if e <= 0 then
        this.timer[timer_id] = nil
        eprintf("periodic start fail: id = %d, e = %d", timer_id, e)
        return -1
    end

    return timer_id
end

-- 停止并删除所有定时器
function Timer.clear()
    local timers = this.timer
    this.timer = {}

    for timer_id, info in pairs(timers) do
        if info.periodic then
            g_thread:periodic_stop(timer_id)
        else
            g_thread:timer_stop(timer_id)
        end
    end

end

local function timer_dispatch(src, udata, timer_id)
    local timer = this.timer[timer_id]
    if not timer then
        eprintf("timer no callback found,abort %d", timer_id)

        -- 应该不会出现。数据没了无法知道是何种类型的定时器，只能都尝试
        g_thread:periodic_stop(timer_id)
        g_thread:timer_stop(timer_id)
        return
    end

    -- wait只需要继续原来的协程
    local session = timer.session
    if session then
        this.timer[timer_id] = nil
        return CoPool.resume(session)
    end

    local times = timer.times
    if times > 0 then -- -1表示无限次数循环
        timer.ts = timer.ts + 1
        if timer.ts >= times then
            this.timer[timer_id] = nil
            if timer.periodic then
                g_thread:periodic_stop(timer_id)
            else
                g_thread:timer_stop(timer_id)
            end
        end
    end

    return CoPool.invoke(timer.cb)
end

ThreadMessage.reg(ThreadMessage.TIMER, timer_dispatch)

return Timer
