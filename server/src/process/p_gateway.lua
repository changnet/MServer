-- 测试进程入口文件(此文件不热更)

Bootstrap.process_init()

-- 当前进程需要启动的worker {线程名, 入口文件名}
local worker_setting = {
    {file = "w_gateway.lua", type = WORKER.GATEWAY, index = 1},
}

Worker.set(worker_setting)
