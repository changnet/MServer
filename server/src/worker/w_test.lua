-- 用于测试的worker入口文件

local addr = ...

local srv_dir = g_env:get("srv_dir")
dofile(srv_dir .. "/src/engine/bootstrap.lua")

Bootstrap.worker_preload(tonumber(addr), "T1")

collectgarbage("stop") -- 测试中有不少性能测试，避免gc影响测试结果

Timer.timeout(0, function()
    require("test.test_load")
end)
