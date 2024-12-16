-- coroutine poll
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

local function co_body()
    local co = co_running()
    local session = next_session()

    co_to_session[co] = session
    session_to_co[session] = co

    while true do
        co_invoke(co, co_yield())

        -- mark as idle
        busy_co[co] = nil
        tableinsert(idle_co, co)
    end
end

-- 从协程库里获取一个空闲协程并执行函数f
function CoPool.invoke(f, ...)
    -- a call table.remove（l) removes the last element of list l
    local co = tableremove(idle_co)
    if not co then
        co = co_create(co_body)
        co_resume(co)
    end

    return co_resume(co, f, ...)
end

-- 根据session恢复协程的执行
function CoPool.resume(session, ...)
    local co = session_to_co[session]
    assert(co and busy_co[co])

    current_co = co
    return co_resume(...)
end

-- 获取当前执行的协程session
function CoPool.current_session()
    return co_to_session[current_co]
end
