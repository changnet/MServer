-- Ev
-- auto export by engine_api.lua do NOT modify!

-- event loop, 事件主循环
local Ev = {}

-- 关闭socket、定时器等并退出循环
function Ev:exit()
end

-- 获取帧时间戳，秒
-- 如果服务器卡了，这时间和实时时间对不上
function Ev:time()
end

-- 获取帧时间戳，毫秒
function Ev:ms_time()
end

-- 进入后台循环
function Ev:backend()
end

-- 手动更新主循环时间，慎用。
-- 一般主循环每个帧都会更新时间，但如果做特殊处理时（例如执行行时间测试），所有逻辑都在一帧内
-- 执行，这时就需要手动更新主循环时间。手动更新时间可能导致一些逻辑错误，如after_run。
function Ev:time_update()
end

-- 查看繁忙的线程
-- @param skip 是否跳过被设置为不需要等待的线程
-- @return 线程名字 已处理完等待交付主线程的任务 等待处理的任务
function Ev:who_busy(skip)
end

-- 获取实时时间戳，秒
function Ev:real_time()
end

-- 获取实时时间戳，毫秒
function Ev:real_ms_time()
end

-- 设置主循环单次循环临界时间，当单次循环超过此时间时，将会打印繁忙日志
-- @param critical 临界时间，毫秒
function Ev:set_critical_time(critical)
end

-- 注册信号处理
-- @param sig 信号，如SIGTERM
-- @param action 处理方式
-- 0删除之前的注册，1忽略此信号，其他则回调到脚本sig_handler处理
function Ev:signal(sig, action)
end

-- 设置app回调时间，不断回调到脚本全局application_ev函数
-- @param interval 回调间隔，毫秒
function Ev:set_app_ev(interval)
end

return Ev
