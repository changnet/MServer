-- 数据缓存及读写worker入口文件

local addr = ...

local source = g_env:get("source")
dofile(source .. "src/engine/startup.lua")

Startup.worker_init(tonumber(addr), "data.data_loader")

