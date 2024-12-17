-- coroutine pool
CoPool = {}

local current_co = nil -- 当前正在执行的协程
local idle_co = {} -- 空闲的协程
local busy_co = {} -- 工作的协程
local session_to_co = {} -- 通过session查找协程
local co_to_session = {} -- 通过协程查找session
local session_seed = 0

local tableremove = table.remove
local tableinsert = table.insert
local co_resume = coroutine.resume
local co_create = coroutine.create
local co_yield = coroutine.yield
local co_running = coroutine.running

local CO_IDLE = "__0xCC_idle" -- 标记协程空闲，尽量不与普通返回值一致

-- 0xcccccccc 标记windows未初始化栈内存即烫烫烫烫
-- 0xcdcdcdcd 标记windows未初始化堆内存即屯屯屯屯

-- 分配下一个session
local function next_session()
    session_seed = session_seed + 1
    if session_seed > 0x7fffffff then
        session_seed = 1
    end

    if not session_to_co[session_seed] then
        return session_seed
    end

    while true do
        session_seed = session_seed + 1
        if session_seed > 0x7fffffff then
            session_seed = 1
        end
        if not session_to_co[session_seed] then
            return session_seed
        end
    end
end

local function co_invoke(co, f, ...)
    -- mark as busy
    current_co = co
    busy_co[co] = 1

    return f(...)
end

-- 处理协程resume的返回值
local function co_return(co, ok, v1, ...)
    --[[
    协程有几种情况会返回：
    1. 运行错误，这时候ok为false
    2. 在其他模块执行了yield，等待返回。这时候应该保持busy状态
    3. 逻辑已执行完，等待分配。这时候应该设置为idle状态

    对于情况23，只能通过yield传参数区分。但情况2实际没法控制，因为第三方库不可能会特意
    多传一个参数CO_IDLE，所以v1参数不为指定值默认为情况2
    ]]

    if not ok then
        -- 协程出错后就变为dead状态了，只能丢弃掉
        busy_co[co] = nil
        local ss = co_to_session[co]

        session_to_co[ss] = nil
        co_to_session[co] = nil
        __G__TRACKBACK(v1, co)
        return false
    elseif v1 == CO_IDLE then
        busy_co[co] = nil
        tableinsert(idle_co, co)
    end

    return true, v1, ...
end

local function co_body()
    local co = co_running()
    local session = next_session()

    co_to_session[co] = session
    session_to_co[session] = co

    while true do
        co_invoke(co, co_yield(CO_IDLE))

        -- 不能在这里处理co，因为co报错时，这里就不会再执行了
        -- busy_co[co] = nil
    end
end

-- 从协程库里获取一个空闲协程并执行函数f
function CoPool.invoke(f, ...)
    -- a call table.remove（l) removes the last element of list l
    local co = tableremove(idle_co)
    if not co then
        co = co_create(co_body)
        local ok = co_resume(co)
        if not ok then co_return(co, false) end
    end

    return co_return(co, co_resume(co, f, ...))
end

-- 根据session恢复协程的执行
function CoPool.resume(session, ...)
    local co = session_to_co[session]
    assert(co and busy_co[co])

    current_co = co
    return co_return(co, co_resume(co, ...))
end

-- 获取当前执行的协程session
function CoPool.current_session()
    return co_to_session[current_co]
end
