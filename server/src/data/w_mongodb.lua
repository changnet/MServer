-- 数据缓存及读写worker入口文件

local addr = ...

local source = g_sharedata:get("source")
dofile(source .. "src/engine/startup.lua")


g_mongodb = nil

Startup.worker_init(tonumber(addr), function(reload)
    require "data.data_loader"
    local MongoDB = require "mongodb.mongodb"

    if not reload then
        g_mongodb = MongoDB()

        local conf = g_setting.mongodb
        g_mongodb:connect_later(conf.ip, conf.port, conf.user, conf.pwd, conf.db)
    end

    Rpc.delegate("MongoDB", g_mongodb, MongoDB.delegate_with_error)
end)
