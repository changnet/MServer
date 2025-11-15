-- 数据缓存及读写worker入口文件

local addr = ...

local srv_dir = g_env:get("srv_dir")
dofile(srv_dir .. "/src/engine/startup.lua")

Startup.worker_init(tonumber(addr), "data.data_loader")
