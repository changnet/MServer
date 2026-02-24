-- 场景worker入口文件

local addr = ...

local source = g_sharedata:get("source")
dofile(source .. "src/engine/startup.lua")

Startup.worker_init(tonumber(addr), "modules.module_loader")
