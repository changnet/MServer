-- 测试进程入口文件(此文件不热更)

Bootstrap.process_preload()

-- 当前进程需要启动的worker {线程名, 入口文件名}
-- 线程名不能超过16个字段
local worker_setting = {
    {file = "w_gateway.lua", type = WORKER_GATEWAY, index = 1},
}

Bootstrap.process_start(worker_setting)
