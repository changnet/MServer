-- Timer
-- auto export by engine_api.lua do NOT modify!

-- 定时器
Timer = {}

-- 设置定时器参数
-- @param after 延迟N毫秒后第一次回调
-- @param repeated 可选参数，每隔M毫秒后循环一次
-- @param time_jump boolean，可选参数，时间跳跃时(比如服务器卡了)是否补偿回调
function Timer:set(after, repeated, time_jump)
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

