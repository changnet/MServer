-- 公用游戏逻辑worker入口文件

local addr = ...

local srv_dir = g_env:get("srv_dir")
dofile(srv_dir .. "/src/engine/bootstrap.lua")

Bootstrap.worker_init(tonumber(addr), "modules.module_loader")
