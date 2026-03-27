-- 系统事件(SystemEvent)
SE = {}

local ev_cb = {}-- 回调函数列表
local reg_cb = {} -- 注册过来的回调列表
local ready = false -- 初始化完成后，不允许再注册事件

require "modules.event.event_header"

-- 注册系统事件回调
-- @param ev 事件id，SE_XXX，详见系统事件定义
-- @param cb 回调函数，回调参数取决于各个事件
-- @param pr 优先级priority，越小优先级越高，默认20
function SE.reg(ev, cb, pr)
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

-- 触发系统事件
-- @param ev 事件id，SE_XXX，详见系统事件定义
-- @param ... 自定义参数
function SE.fire(ev, ...)
    local cbs = ev_cb[ev]
    if not cbs then return end

    for _, cb in pairs(cbs) do
        -- TODO 暂时不用xpcall，影响性能，用的话容错要好一些
        cb(...)
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

return SE
