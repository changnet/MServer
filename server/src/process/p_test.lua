-- 测试进程入口文件(此文件不热更)



PROCESS_ID  = PROCESS_TEST
LOCAL_ADDR = Engine.make_address(PROCESS_ID, 0, 1)

print("test process start, address", LOCAL_ADDR)
g_env:set("process_id", PROCESS_ID)

Signal.mask(2, Shutdown.begin)
Signal.mask(15, Shutdown.begin)

-- 当前进程需要启动的worker {线程名, 入口文件名}
-- 线程名不能超过16个字段
local worker_setting = {
    {name = "test", file = "w_test.lua", type = WORKER_TEST, index = 1},
    -- {name = "test2", file = "w_test.lua", type = WORKER_TEST, index = 2},
}

Bootstrap.process_start(worker_setting)
