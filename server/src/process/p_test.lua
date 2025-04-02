-- 测试进程入口文件(此文件不热更)

Bootstrap.process_init()

-- 当前进程需要启动的worker {线程名, 入口文件名}
-- 线程名不能超过16个字段
local worker_setting = {
    {file = "w_test.lua", type = WORKER.TEST, index = 1},
    -- {file = "w_test.lua", type = WORKER.TEST, index = 2},
}

Worker.set(worker_setting)

Bootstrap.start()
