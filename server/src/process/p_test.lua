-- 测试进程入口文件(此文件不热更)

local WorkerThread = require "engine.WorkerThread"

PROCESS_ID  = PROCESS_TEST
LOCAL_ADDR = Engine.make_address(PROCESS_ID, 0, 1)

print("test process start, address", LOCAL_ADDR)
g_env:set("process_id", PROCESS_ID)

Signal.mask(2, Engine.exit)
Signal.mask(15, Engine.exit)

-- 当前进程需要启动的worker {线程名, 入口文件名}
-- 线程名不能超过16个字段
local worker_setting = {
    {"test", "w_test.lua"},
    -- {"test2", "test"},
}

-- lua加载文件可以自动识别/和\，所以这里统一用linux的就行
local base_path = g_env:get("srv_dir") .. "/src/worker/"
for _, s in pairs(worker_setting) do
    local w = WorkerThread(s[1])
    w:start(base_path .. s[2])
end
