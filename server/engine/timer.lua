-- Timer
-- auto export by engine_api.lua do NOT modify!

-- 定时器
local Timer = {}

-- 设置定时器参数
-- @param after 延迟N毫秒后第一次回调
-- @param repeated 可选参数，每隔M毫秒后循环一次
-- @param policy int，可选参数，当时间出现偏差时补偿策略，见 EVTimer::Policy
function Timer:set(after, repeated, policy)
end

-- 停止定时器
function Timer:stop()
end

-- 启动定时器
function Timer:start()
end

-- 定时器是否启动
-- @return boolean，是否启动
function Timer:active()
end

return Timer
