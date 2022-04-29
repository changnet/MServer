-- 玩家事件
PE = {}

local ev_cb = {}

require "modules.event.event_header"

-- 注册系统事件回调
-- @param ev 事件id，PE_XXX，详见玩家事件定义
-- @param cb 回调函数，回调参数取决于各个事件，可自动回调到player的子模块
-- @param pr 优先级priority，越小优先级越高，默认20
function PE.reg(ev, cb, pr)
    local cbs = ev_cb[ev]
    if not cbs then
        cbs = {}
        ev_cb[ev] = cbs
    end

    assert(cb and not g_app.ready)

    -- 默认优先级20
    -- 为啥是20，因为linux下top列出的就是20，照抄
    return table.insert(cbs, {cb, pr or 20})
end

-- 触发玩家事件
-- @param ev 事件id，PE_XXX，详见玩家事件定义
-- @param ... 自定义参数
function PE.fire_event(ev, ...)
    local cbs = ev_cb[ev]
    if not cbs then return end

    for _, cb in pairs(cbs) do
        -- TODO 暂时不用xpcall，影响性能，用的话容错要好一些
        cb(...)
    end
end

-- 生成回调函数
local function make_cb()
    local ThisCall = require "modules.system.this_call"

    for ev, cbs in pairs(ev_cb) do
        -- pr值越小，优先级越高
        table.sort(cbs, function(a, b) return a[2] < b[2] end)

        local funcs = {}
        local check = {} -- 用于检测同一个函数重复注册
        for _, cb in pairs(cbs) do
            local cb_func = cb[1]
            assert(not check[cb_func], ev)

            check[cb_func] = true
            local func = ThisCall.make_from_player(cb_func, nil, "PE", ev)
            table.insert(funcs, func or cb_func)
        end

        -- 转换为一个函数数组，避免回调时再执行一次hash取函数指针
        ev_cb[ev] = funcs
    end
end

SE.reg(SE_SCRIPT_LOADED, make_cb, 10)

return PE
