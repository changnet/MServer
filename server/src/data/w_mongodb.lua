-- 数据缓存及读写worker入口文件

local addr = ...

local srv_dir = g_env:get("srv_dir")
dofile(srv_dir .. "/src/engine/startup.lua")


g_mongodb = nil

Startup.worker_init(tonumber(addr), function()
    Startup.use_loader("modules.module_loader")


    local MongoDB = require "mongodb.mongodb"
    g_mongodb = MongoDB()
    Rpc.delegate("MongoDB", g_mongodb)

    local conf = g_setting.mongodb
    g_mongodb:connect_later(conf.ip, conf.port, conf.user, conf.pwd, conf.db)
end)
