-- 事件(Event)
Event = {}

local ev_cb = {}-- 回调函数列表
local reg_cb = {} -- 注册过来的回调列表
local ready = false -- 初始化完成后，不允许再注册事件

local scall = scall
local assert = assert
local LOCAL_TYPE = LOCAL_TYPE
local has_pobj = (1 == WORKER[LOCAL_TYPE].pobj)

require "modules.event.event_header"

local WORKER_LIST = {}
for k, e in pairs(EV) do
    local id = e.i

    EV[k] = id
    if e.w then WORKER_LIST[id] = e.w end
end

-- 注册事件回调
-- @param ev EV 事件id，详见系统事件定义
-- @param cb func 回调函数，回调参数取决于各个事件
-- @param pr number 优先级priority，越小优先级越高，默认20
function Event.reg(ev, cb, pr)
    local cbs = reg_cb[ev]
    if not cbs then
        cbs = {}
        reg_cb[ev] = cbs
    end

    assert(cb and not ready) -- 生成回调函数后，不允许再注册事件

    -- 默认优先级20
    -- 为啥是20，因为linux下top列出的就是20，照抄
    return table.insert(cbs, {cb, pr or 20})
end

local function emit(ev, ...)
    local cbs = ev_cb[ev]
    if not cbs then return end

    for _, cb in pairs(cbs) do
        scall(cb, ...)
    end
end

-- 触发系统事件
-- @param ev 事件id，EV.XXX，详见event_header
-- @param ... 自定义参数(需要转发的事件，禁止传复杂table)
function Event.semit(ev, ...)
    emit(ev, ...)

    local list = WORKER_LIST[ev]
    if not list then return end

    -- 玩家事件包含player对象，直接把整个player对象通过rpc发过去就完蛋
    assert(ev > 1024)

    for _, wtype in pairs(list) do
        Worker.send_other_type(wtype, Event.on_semit, ev, ...)
    end
end

-- 触发玩家事件，第一个参数为player
-- @param ev 事件id，EV.XXX，详见系统事件定义
-- @param ... 自定义参数
function Event.pemit(player, ev, ...)
    emit(ev, player, ...)

    local list = WORKER_LIST[ev]
    if not list then return end

    local pid = player.pid
    for _, wtype in pairs(list) do
        local addr = Router.find_player_addr(pid, wtype)
        Send[addr].Event.on_pemit(pid, ev, ...)
    end
end

-- 触发事件，如果某个函数不返回true则终止
-- @param ev 事件id，EV.XXX，详见玩家事件定义
-- @param ... 自定义参数
function Event.pemit_true(player, ev, ...)
    local cbs = ev_cb[ev]
    if not cbs then return true end

    for _, cb in pairs(cbs) do
        if not scall(cb, player, ...) then return false end
    end

    -- 暂时没有事件需要异步到其他线程获取的需求
    assert(not WORKER_LIST[ev])

    return true
end

-- 其他worker收到事件派发
function Event.on_semit(pid, ev, ...)
    return emit(ev, ...)
end

-- 其他worker收到玩家事件派发
function Event.on_pemit(pid, ev, ...)
    -- 如果这个worker有对象，则使用对象触发，否则以pid触发
    if has_pobj then
        local player = PlayerMgr.get_player(pid)
        if not player then
            eprint("event on psemit no player found", pid)
            return
        end
        return emit(ev, player, ...)
    else
        return emit(ev, ...)
    end
end

-- 标记系统事件已就绪，后续不再允许注册事件
local function sort_events()
    assert(not ready)

    ready = true
    for ev, cbs in pairs(reg_cb) do
        -- pr值越小，优先级越高
        table.sort(cbs, function(a, b) return a[2] < b[2] end)

        local funcs = {}
        local check = {} -- 用于检测同一个函数重复注册
        for _, cb in pairs(cbs) do
            local cb_func = cb[1]
            assert(not check[cb_func], ev)

            check[cb_func] = true
            table.insert(funcs, cb_func)
        end

        -- 转换为一个函数数组，避免回调时再执行一次hash取函数指针
        ev_cb[ev] = funcs
    end
end

script_loaded(sort_events)

return Event
