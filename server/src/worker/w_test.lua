-- 用于测试的worker入口文件

local addr = ...

local srv_dir = g_env:get("srv_dir")
dofile(srv_dir .. "/src/engine/bootstrap.lua")

Bootstrap.worker_preload(tonumber(addr), "T1")

Rtti.collect()

print("worker test ready", addr)
