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
    -- return co_invoke(co, co_yield(CO_IDLE, f(...))) -- 递归方案
end

local function co_body()
    local co = co_running()
    local session = next_session()

    co_to_session[co] = session
    session_to_co[session] = co

    --[[
    协程池执行的函数f和参数是通过yield传过来的，而协程挂起时，返回的值需要作为yield的参数
    这属于一个嵌套依赖，鸡生蛋还是蛋生鸡的问题

    使用循环就没法正确返回值
    在co_invoke使用递归可以，但会堆栈溢出。幸好Lua的尾调用可以解决堆栈溢出

    一般情况下使用协程并不需要返回值，所以用循环的方案更好？

    2026更新：
    虽然递归方案看起来能正确返回值，但那是在协程内部没有yield的情况下。
    如果协程内部有yield，那么co_invoke的返回值就是yield的返回值，而不是f的返回值。
    所以递归方案并不能正确返回值。因此这里也有必要使用递归方案了。
    ]]

    while true do
        co_invoke(co, co_yield(CO_IDLE))
    end

    -- co_invoke(co, co_yield(CO_IDLE)) -- 递归方案
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

        __G_DUMP_STACK(v1, co, 0)
        return false
    elseif v1 == CO_IDLE then
        busy_co[co] = nil
        tableinsert(idle_co, co)

        return true
    end

    return true, v1, ...
end

-- 从协程库里获取一个空闲协程并执行函数f
-- @return 返回是否成功(true or false), 函数f返回的其他参数（如果函数f yield，则返回yield的参数）
function CoPool.invoke(f, ...)
    -- a call table.remove（l) removes the last element of list l
    local co = tableremove(idle_co)
    if not co then
        co = co_create(co_body)
        local ok = co_resume(co) -- 执行co_body，把自己放到协程池
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
-- 注意：此函数不能用于业务逻辑，因为一个函数里可能发起另一个协程，获取的session会变
-- 它仅用于调试（比如当前在执行哪个协程）
function CoPool.current_session()
    return co_to_session[current_co]
end

-- 获取空闲的协程数量
function CoPool.idle_count()
    return #idle_co
end

-- 获取使用中的协程数量
function CoPool.busy_count()
    return table.size(busy_co)
end
