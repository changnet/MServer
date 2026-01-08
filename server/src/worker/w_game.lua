-- 公用游戏逻辑worker入口文件

local addr = ...

local source = g_env:get("source")
dofile(source .. "src/engine/startup.lua")

Startup.worker_init(tonumber(addr), "modules.module_loader")
