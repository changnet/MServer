-- 日志写入worker入口文件

local addr = ...

local source = g_sharedata:get("source")
dofile(source .. "src/engine/startup.lua")

Startup.worker_init(tonumber(addr), "log.log_loader")
