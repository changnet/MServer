-- 系统事件(SystemEvent)
SE = {}

local ev_cb = {}

require "modules.event.event_header"

-- 注册系统事件回调
-- @param ev 事件id，SE_XXX，详见系统事件定义
-- @param cb 回调函数，回调参数取决于各个事件
function SE.reg(ev, cb)
    -- TODO 这里是否需要弄个优先级？如果用得不多，定义多个事件也可以
    if not ev_cb[ev] then ev_cb[ev] = {} end
    table.insert(ev_cb[ev], cb)
end

-- 触发系统事件
-- @param ev 事件id，SE_XXX，详见系统事件定义
-- @param ... 自定义参数
function SE.fire_event(ev, ...)
    local cbs = ev_cb[ev]
    if not cbs then return end

    for _, cb in pairs(cbs) do
        -- TODO 暂时不用xpcall，影响性能，用的话容错要好一些
        cb(...)
    end
end

return SE
