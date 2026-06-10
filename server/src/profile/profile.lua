-- 性能分析
Profile = {}

local util = require("engine.util")

-- @class TimingObj 单条时间记录结构
-- @field key string 唯一标识
-- @field times integer 记录的总次数
-- @field time integer 总耗时（毫秒）
-- @field max integer 单次最大耗时
-- @field min integer 单次最小耗时

-- @class TimingCtx 记录时间消耗的上下文
-- @field name string 模块名
-- @field alert_ms integer 超过N毫秒则打印警告日志
-- @field beg_tm integer 开始记录时间戳
-- @field list table<string, TimingObj> 模块名
-- @field init_tm integer 初始化时间戳
-- @field yield_tm integer 挂起开始时间
-- @field yield_total integer 挂起总时间
-- @field co thread 绑定的协程

local co_hash -- [co] = ctx 当前挂起的协程
local ctx_hash -- [name] = ctx 已创建的模块数据

local ENABLE_PROFILE = g_debug
local steady_clock = Engine.steady_clock
local coroutine_running = coroutine.running

local function get_ctx(name)
    local ctx = ctx_hash[name]
    if not ctx then
        ctx = {
            name = name,
            list = {},
            init_tm = os.time(),
            yield_total = 0,
        }
        ctx_hash[name] = ctx
    end

    return ctx
end

-- 初始化上下文
-- @param name string 模块名
-- @param alert_ms integer 超过N毫秒则打印警告日志
-- @return TimingCtx
function Profile.init_timing_context(name, alert_ms)
    local ctx = get_ctx(name)
    ctx.alert_ms = alert_ms
    return ctx
end

-- 开始计算时间
-- @param name string 统计名字
-- @param ignore_co boolean 是否忽略协程挂起时间
function Profile.begin_timing(name, ignore_co)
    local ctx = get_ctx(name)
    ctx.beg_tm = steady_clock()
    ctx.yield_total = 0

    if ignore_co then
        local co = coroutine_running()
        co_hash[co] = ctx
    end
end

-- 结束计算时间
-- @param name string 统计名字
-- @param key string 本次统计的唯一key
-- @param ... 如果超过alert_ms时，打印的额外参数
function Profile.end_timing(name, key, ...)
    local ctx = get_ctx(name)
    if not ctx.beg_tm then return end

    local now = steady_clock()
    local cost = now - ctx.beg_tm - ctx.yield_total

    local obj = ctx.list[key]
    if not obj then
        obj = {
            key = key,
            times = 0,
            time = 0,
            max = 0,
            min = 999999999,
        }
        ctx.list[key] = obj
    end

    obj.times = obj.times + 1
    obj.time = obj.time + cost
    if cost > obj.max then obj.max = cost end
    if cost < obj.min then obj.min = cost end

    if ctx.alert_ms and cost >= ctx.alert_ms then
        Log.warn("profile timing alert: %s %s %dms args:", name, key, cost, ...)
    end

    ctx.beg_tm = nil

    local co = coroutine_running()
    co_hash[co] = nil
end

-- 把统计数据写入日志文件
-- @param name string 统计名字
-- @param reset boolean 写入后是否重置统计信息
function Profile.log_timing(name, reset)
    local ctx = ctx_hash[name]
    if not ctx then
        eprint("no profile context found", name)
        return
    end

    local list = {}
    for _, obj in pairs(ctx.list) do
        table.insert(list, obj)
    end

    table.sort(list, function(a, b)
        return a.time > b.time
    end)

    util.mkdir_p("profile")

    local now_str = os.date("%Y%m%d%H%M%S")
    local filename = string.format(
        "profile/timing_%s_%s_%s.txt", LOCAL_NAME, name, now_str)
    local fd = io.open(filename, "w")
    if not fd then
        eprint("profile log timing open file fail", filename)
        return
    end

    fd:write(string.format("Profile Report: %s from %s ~ %s\n",
        name, os.date("%Y%m%d%H%M%S", ctx.init_tm), now_str))
    fd:write("--------------------------------------------------------------\n")
    fd:write(string.format("%-30s | %-10s | %-10s | %-10s | %-10s | %-10s\n",
        "Key", "Time(ms)", "Times", "Avg(ms)", "Min(ms)", "Max(ms)"))
    fd:write("--------------------------------------------------------------\n")

    local t_times = 0
    local t_min = 0xFFFFFFFF
    local t_max = 0
    local t_time = 0
    for _, obj in ipairs(list) do
        local time = obj.time
        local times = obj.times
        local avg = times > 0 and (time / times) or 0
        fd:write(string.format("%-30s | %-10d | %-10d | %-10.2f | %-10d | %-10d\n",
            obj.key, time, times, avg, obj.min, obj.max))

        t_times = t_times + times
        t_time = t_time + obj.time
        if obj.min < t_min then t_min = obj.min end
        if obj.max > t_max then t_max = obj.max end
    end
    fd:write("--------------------------------------------------------------\n")
    local t_avg = t_times > 0 and t_time/t_times or 0
    fd:write(string.format("%-30s | %-10d | %-10d | %-10.2f | %-10d | %-10d\n",
        "total", t_time, t_times, t_avg, t_min, t_max))

    fd:close()

    print("write profile to", filename)
    if reset then
        ctx.list = {}
        ctx.init_tm = os.time()
    end
end

if ENABLE_PROFILE then
    if not _G.__profile then
        _G.__profile = {
            co_hash = setmetatable({}, { __mode = "k" }),
            ctx_hash = {}
        }
    end
    co_hash = _G.__profile.co_hash
    ctx_hash = _G.__profile.ctx_hash

    local yield = coroutine.yield
    local resume = coroutine.resume

    function coroutine.yield(...)
        local co = coroutine_running()
        local ctx = co_hash[co]
        if ctx then
            ctx.yield_tm = steady_clock()
        end
        return yield(...)
    end

    function coroutine.resume(co, ...)
        local ctx = co_hash[co]
        if ctx then
            local now = steady_clock()
            ctx.yield_total = ctx.yield_total + (now - ctx.yield_tm)
            ctx.yield_tm = nil
        end
        return resume(co, ...)
    end
end

return Profile
