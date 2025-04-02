-- 数据缓存及读写worker入口文件

local addr = ...

local srv_dir = g_env:get("srv_dir")
dofile(srv_dir .. "/src/engine/bootstrap.lua")

Bootstrap.worker_init(tonumber(addr))

Timer.timeout(0, function()
    require("modules.module_loader")
end)
